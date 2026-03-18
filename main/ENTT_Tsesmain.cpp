// =============================================================================
//  EnTT 3.16  機能網羅サンプル
//  ゲームのフレームを想定したシンプルな構成で全機能を実演
// =============================================================================
#include "entt/entt.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  コンポーネント定義
// ─────────────────────────────────────────────────────────────────────────────
struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp, maxHp; };
struct Tag { std::string name; };
struct Disabled {};   // フラグコンポーネント（データなし）

// ─────────────────────────────────────────────────────────────────────────────
//  イベント定義
// ─────────────────────────────────────────────────────────────────────────────
struct EnemyDeathEvent { entt::entity entity; int scoreReward; };
struct DamageEvent { entt::entity target; int amount; };

// ─────────────────────────────────────────────────────────────────────────────
//  セクション区切り出力ヘルパー
// ─────────────────────────────────────────────────────────────────────────────
static void PrintSection(const std::string& title)
{
    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout << "  " << title << "\n";
    std::cout << "╚══════════════════════════════════════╝\n";
}

// =============================================================================
//  1. registry ─ エンティティ & コンポーネント
// =============================================================================
static void Demo_Registry()
{
    PrintSection("1. registry");

    entt::registry registry;

    // ── エンティティ生成 ──────────────────────────────────────────
    auto player = registry.create();
    auto enemy1 = registry.create();
    auto enemy2 = registry.create();

    // ── emplace : コンポーネント追加 ─────────────────────────────
    registry.emplace<Position>(player, 0.f, 0.f);
    registry.emplace<Velocity>(player, 1.f, 0.f);
    registry.emplace<Health>(player, 100, 100);
    registry.emplace<Tag>(player, "Player");

    registry.emplace<Position>(enemy1, 10.f, 5.f);
    registry.emplace<Health>(enemy1, 30, 30);
    registry.emplace<Tag>(enemy1, "Enemy_A");

    registry.emplace<Position>(enemy2, 20.f, 0.f);
    registry.emplace<Health>(enemy2, 50, 50);
    registry.emplace<Tag>(enemy2, "Enemy_B");
    registry.emplace<Disabled>(enemy2);         // 無効フラグ

    // ── get : 参照取得 ────────────────────────────────────────────
    auto& playerPos = registry.get<Position>(player);
    std::cout << "[get] Player pos: " << playerPos.x << ", " << playerPos.y << "\n";

    // ── try_get : ポインタ取得（なければ nullptr）────────────────
    auto* vel = registry.try_get<Velocity>(enemy1);
    std::cout << "[try_get] Enemy1 has Velocity? " << (vel ? "yes" : "no") << "\n";

    // ── patch : ラムダで直接変更 ─────────────────────────────────
    registry.patch<Position>(player, [](auto& p) { p.x += 5.f; p.y += 3.f; });
    auto [px, py] = registry.get<Position>(player);
    std::cout << "[patch] Player moved to: " << px << ", " << py << "\n";

    // ── replace : 値ごと置き換え ──────────────────────────────────
    registry.replace<Health>(enemy1, 10, 30);

    // ── get_or_emplace : なければ生成 ────────────────────────────
    auto& newVel = registry.get_or_emplace<Velocity>(enemy1, 2.f, 1.f);
    std::cout << "[get_or_emplace] Enemy1 velocity: " << newVel.vx << ", " << newVel.vy << "\n";

    // ── all_of / any_of ───────────────────────────────────────────
    bool hasAll = registry.all_of<Position, Health>(enemy1);
    bool hasAny = registry.any_of<Velocity, Disabled>(enemy2);
    std::cout << "[all_of] Enemy1 has Position+Health: " << hasAll << "\n";
    std::cout << "[any_of] Enemy2 has Velocity or Disabled: " << hasAny << "\n";

    // ── orphan : コンポーネントゼロか ────────────────────────────
    auto ghost = registry.create();
    std::cout << "[orphan] ghost has no components: " << registry.orphan(ghost) << "\n";

    // ── remove / erase ────────────────────────────────────────────
    registry.remove<Disabled>(enemy2);   // なくても安全
    registry.erase<Velocity>(enemy1);   // 必ず存在すること

    // ── ライフサイクルシグナル ────────────────────────────────────
    registry.on_construct<Health>().connect < +[](entt::registry&, entt::entity e)
        {
            std::cout << "[signal] Health constructed on entity " << (uint32_t)e << "\n";
        } > ();

    auto newEnemy = registry.create();
    registry.emplace<Health>(newEnemy, 20, 20);  // シグナルが発火する

    // ── destroy ───────────────────────────────────────────────────
    registry.destroy(ghost);
    std::cout << "[valid] ghost after destroy: " << registry.valid(ghost) << "\n";
}

// =============================================================================
//  2. view ─ コンポーネントでエンティティを列挙
// =============================================================================
static void Demo_View()
{
    PrintSection("2. view");

    entt::registry registry;

    auto a = registry.create();
    auto b = registry.create();
    auto c = registry.create();
    registry.emplace<Position>(a, 1.f, 1.f); registry.emplace<Velocity>(a, 1.f, 0.f);
    registry.emplace<Position>(b, 2.f, 2.f); registry.emplace<Velocity>(b, 0.f, 1.f);
    registry.emplace<Position>(c, 3.f, 3.f); // Velocity なし

    // ── 複数コンポーネント view ───────────────────────────────────
    std::cout << "[view] Entities with Position + Velocity:\n";
    auto moveView = registry.view<Position, Velocity>();
    for (auto [entity, pos, vel] : moveView.each())
    {
        pos.x += vel.vx;
        pos.y += vel.vy;
        std::cout << "  entity=" << (uint32_t)entity
            << " pos=(" << pos.x << "," << pos.y << ")\n";
    }

    // ── exclude フィルタ ──────────────────────────────────────────
    registry.emplace<Disabled>(b);
    std::cout << "[view+exclude] Position only, excluding Disabled:\n";
    auto filteredView = registry.view<Position>(entt::exclude<Disabled>);
    for (auto [entity, pos] : filteredView.each())
        std::cout << "  entity=" << (uint32_t)entity << "\n";
}

// =============================================================================
//  3. group ─ 所有権による高速グループ
// =============================================================================
static void Demo_Group()
{
    PrintSection("3. group");

    entt::registry registry;

    for (int i = 0; i < 5; ++i)
    {
        auto e = registry.create();
        registry.emplace<Position>(e, (float)i, 0.f);
        registry.emplace<Velocity>(e, 1.f, 0.f);
        if (i % 2 == 0) registry.emplace<Health>(e, 50, 50);
    }

    // owned<Position, Velocity>: この2コンポーネントのメモリが連続配置される
    auto group = registry.group<Position, Velocity>(entt::get<Health>);
    std::cout << "[group] Entities owning Position+Velocity, also having Health:\n";
    for (auto [entity, pos, vel, hp] : group.each())
    {
        std::cout << "  entity=" << (uint32_t)entity
            << " hp=" << hp.hp << "\n";
    }

    // ── sort（グループのソート）────────────────────────────────────
    group.sort<Position>([](const Position& a, const Position& b) { return a.x > b.x; });
    std::cout << "[group.sort] Sorted by x desc:\n";
    for (auto [entity, pos, vel] : registry.view<Position, Velocity>().each())
        std::cout << "  entity=" << (uint32_t)entity << " x=" << pos.x << "\n";
}

// =============================================================================
//  4. handle ─ entity + registry のラッパー
// =============================================================================
static void Demo_Handle()
{
    PrintSection("4. handle");

    entt::registry registry;
    auto entity = registry.create();

    entt::handle handle{ registry, entity };

    handle.emplace<Position>(5.f, 10.f);
    handle.emplace<Tag>("HandleEntity");

    auto& pos = handle.get<Position>();
    std::cout << "[handle] pos: " << pos.x << ", " << pos.y << "\n";
    std::cout << "[handle] tag: " << handle.get<Tag>().name << "\n";

    handle.patch<Position>([](auto& p) { p.x = 99.f; });
    std::cout << "[handle.patch] new x: " << handle.get<Position>().x << "\n";

    // handle 経由で destroy
    handle.destroy();
    std::cout << "[handle.destroy] valid: " << registry.valid(entity) << "\n";
}

// =============================================================================
//  5. dispatcher ─ イベントバス
// =============================================================================
struct ScoreSystem
{
    int totalScore = 0;
    void OnEnemyDeath(const EnemyDeathEvent& e)
    {
        totalScore += e.scoreReward;
        std::cout << "[ScoreSystem] +score " << e.scoreReward
            << " total=" << totalScore << "\n";
    }
};

struct EffectSystem
{
    void OnEnemyDeath(const EnemyDeathEvent& e)
    {
        std::cout << "[EffectSystem] Explosion on entity "
            << (uint32_t)e.entity << "\n";
    }
    void OnDamage(const DamageEvent& e)
    {
        std::cout << "[EffectSystem] HitEffect on entity "
            << (uint32_t)e.target << " damage=" << e.amount << "\n";
    }
};

static void Demo_Dispatcher()
{
    PrintSection("5. dispatcher");

    entt::dispatcher dispatcher;
    ScoreSystem score;
    EffectSystem effect;

    // ── 接続 ─────────────────────────────────────────────────────
    dispatcher.sink<EnemyDeathEvent>().connect<&ScoreSystem::OnEnemyDeath>(score);
    dispatcher.sink<EnemyDeathEvent>().connect<&EffectSystem::OnEnemyDeath>(effect);
    dispatcher.sink<DamageEvent>().connect<&EffectSystem::OnDamage>(effect);

    entt::registry registry;
    auto enemy = registry.create();

    // ── trigger : 即時発火 ────────────────────────────────────────
    std::cout << "[trigger]\n";
    dispatcher.trigger(EnemyDeathEvent{ enemy, 200 });

    // ── enqueue + update : キューに積んで後でまとめて発火 ─────────
    std::cout << "[enqueue → update]\n";
    dispatcher.enqueue<EnemyDeathEvent>(enemy, 50);
    dispatcher.enqueue<DamageEvent>(enemy, 30);
    std::cout << "  (キューに積んだ時点では何も起きない)\n";
    dispatcher.update();   // 全型を発火

    // ── 特定型だけ発火 ────────────────────────────────────────────
    dispatcher.enqueue<EnemyDeathEvent>(enemy, 999);
    dispatcher.enqueue<DamageEvent>(enemy, 10);
    dispatcher.update<EnemyDeathEvent>();  // DamageEvent はまだ残る
    std::cout << "[update<T>] DamageEventはまだ残っている\n";
    dispatcher.clear<DamageEvent>();       // 残ったキューを捨てる

    // ── 切断 ──────────────────────────────────────────────────────
    dispatcher.sink<EnemyDeathEvent>().disconnect<&ScoreSystem::OnEnemyDeath>(score);
    std::cout << "[disconnect] ScoreSystem切断後にtrigger\n";
    dispatcher.trigger(EnemyDeathEvent{ enemy, 100 });  // EffectSystem のみ反応
}

// =============================================================================
//  6. sigh / sink ─ 汎用シグナル・スロット
// =============================================================================
static void OnGlobalEvent(int value)
{
    std::cout << "[sigh] free function received: " << value << "\n";
}

struct Listener
{
    void OnEvent(int value)
    {
        std::cout << "[sigh] member function received: " << value << "\n";
    }
};

static void Demo_Sigh()
{
    PrintSection("6. sigh / sink");

    entt::sigh<void(int)> signal;
    entt::sink sink{ signal };

    Listener listener;

    // ── 接続 ─────────────────────────────────────────────────────
    sink.connect<&OnGlobalEvent>();
    sink.connect<&Listener::OnEvent>(listener);

    // ── 発火 ─────────────────────────────────────────────────────
    signal.publish(42);

    // ── 切断 ─────────────────────────────────────────────────────
    sink.disconnect<&OnGlobalEvent>();
    std::cout << "[disconnect] free function切断後:\n";
    signal.publish(99);  // listener のみ反応
}

// =============================================================================
//  7. emitter ─ オブジェクト単位のイベント送出
// =============================================================================
struct ClickEvent { float x, y; };
struct HoverEvent { float x, y; };

struct Button : entt::emitter<Button>
{
    std::string label;
    explicit Button(std::string lbl) : label(std::move(lbl)) {}

    void Click(float x, float y) { publish(ClickEvent{ x, y }); }
    void Hover(float x, float y) { publish(HoverEvent{ x, y }); }
};

static void Demo_Emitter()
{
    PrintSection("7. emitter");

    Button btn("OK");

    btn.on<ClickEvent>([](ClickEvent& e, Button& self)
        {
            std::cout << "[emitter] Button \"" << self.label
                << "\" clicked at (" << e.x << "," << e.y << ")\n";
        });
    btn.on<HoverEvent>([](HoverEvent& e, Button& self)
        {
            std::cout << "[emitter] Button \"" << self.label
                << "\" hovered at (" << e.x << "," << e.y << ")\n";
        });

    btn.Click(100.f, 200.f);
    btn.Hover(110.f, 205.f);

    // ── contains / erase ─────────────────────────────────────────
    std::cout << "[contains] HoverEvent: " << btn.contains<HoverEvent>() << "\n";
    btn.erase<HoverEvent>();
    std::cout << "[erase] HoverEvent後のcontains: " << btn.contains<HoverEvent>() << "\n";
    btn.Hover(0.f, 0.f);  // ハンドラが消えているので何も起きない
}

// =============================================================================
//  8. scheduler ─ プロセスのライフサイクル管理
// =============================================================================
// EnTT 3.16: entt::process = basic_process<uint32_t>
// CRTP ではなく仮想関数を override する設計に変わった
// delta_type = uint32_t（ミリ秒などの整数を想定）
// float を使いたい場合は entt::basic_process<float> を継承する

struct FadeInProcess : entt::basic_process<float>
{
    // scheduler.attach が std::allocate_shared<T>(alloc, alloc) で呼ぶため
    // 基底クラスのコンストラクタ（allocator_type を受け取る版）を継承する
    using entt::basic_process<float>::basic_process;

    float elapsed = 0.f;
    float duration = 1.0f;   // 1秒でフェードイン完了

    void update(const float dt, void*) override
    {
        elapsed += dt;
        float alpha = std::min(elapsed / duration, 1.f);
        std::cout << "[FadeIn] alpha=" << alpha << "\n";
        if (alpha >= 1.f) succeed();
    }

    void succeeded() override { std::cout << "[FadeIn] complete!\n"; }
};

struct LogProcess : entt::basic_process<float>
{
    using entt::basic_process<float>::basic_process; // 同上

    void update(const float /*dt*/, void*) override
    {
        std::cout << "[LogProcess] running after FadeIn\n";
        succeed();
    }
    void succeeded() override { std::cout << "[LogProcess] complete!\n"; }
};

static void Demo_Scheduler()
{
    PrintSection("8. scheduler");

    // basic_scheduler<float> を明示（process の Delta に合わせる）
    entt::basic_scheduler<float> scheduler;

    // attach<T>() でプロセスを登録、then<T>() でチェーン接続
    // attach の戻り値は basic_process& なので then を連鎖できる
    scheduler.attach<FadeInProcess>()
        .then<LogProcess>();

    // フレームを模擬（0.4秒 × 4フレーム）
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "-- frame " << i << " --\n";
        scheduler.update(0.4f);
    }
}

// =============================================================================
//  9. organizer ─ システム依存グラフの自動構築
// =============================================================================
static void SystemPhysics(entt::view<entt::get_t<Position, Velocity>> view)
{
    for (auto [e, pos, vel] : view.each())
    {
        pos.x += vel.vx;
        pos.y += vel.vy;
    }
    std::cout << "[organizer] Physics system executed\n";
}

static void SystemRenderer(entt::view<entt::get_t<const Position>> view)
{
    for (auto [e, pos] : view.each())
        (void)pos;  // 描画処理のプレースホルダ
    std::cout << "[organizer] Renderer system executed\n";
}

static void Demo_Organizer()
{
    PrintSection("9. organizer");

    entt::registry registry;
    auto e = registry.create();
    registry.emplace<Position>(e, 0.f, 0.f);
    registry.emplace<Velocity>(e, 1.f, 1.f);

    entt::organizer organizer;

    // Physics は Position / Velocity を書き込む
    organizer.emplace<&SystemPhysics>("Physics");
    // Renderer は Position を読み取るだけ（Physics の後になる）
    organizer.emplace<&SystemRenderer>("Renderer");

    // グラフを取得してトポロジカル順に実行
    auto graph = organizer.graph();
    std::cout << "[organizer] Execution order:\n";
    for (auto& node : graph)
    {
        std::cout << "  -> " << node.name() << "\n";
        node.callback()(node.data(), registry);
    }
}

// =============================================================================
//  10. snapshot ─ シリアライズ / デシリアライズ
// =============================================================================
// 最小限のシリアライザ（std::vector<uint8_t> への書き出し / 読み込み）
struct ByteOutput
{
    std::vector<uint8_t> buffer;

    template<typename T>
    void operator()(const T& value)
    {
        const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }
    // entt の snapshot は (entity[], component[]) を書き込む
    void operator()(const uint32_t size) { (*this)(size); }
};

struct ByteInput
{
    const std::vector<uint8_t>& buffer;
    std::size_t cursor = 0;

    template<typename T>
    void operator()(T& value)
    {
        std::memcpy(&value, buffer.data() + cursor, sizeof(T));
        cursor += sizeof(T);
    }
};

static void Demo_Snapshot()
{
    PrintSection("10. snapshot / snapshot_loader");

    // ── 保存元 registry ──────────────────────────────────────────
    entt::registry src;
    auto e1 = src.create();
    auto e2 = src.create();
    src.emplace<Position>(e1, 3.f, 7.f);
    src.emplace<Velocity>(e1, 1.f, 0.f);
    src.emplace<Position>(e2, 9.f, 2.f);

    // ── バイナリへ書き出し ────────────────────────────────────────
    std::vector<uint8_t> buf;
    {
        // entt::snapshot は archive に (size, data...) の形で書き込む
        // ここでは簡易的に ostringstream を使う
        std::ostringstream oss;
        auto out = [&oss](auto v) { oss.write(reinterpret_cast<const char*>(&v), sizeof(v)); };

        entt::snapshot{ src }
            .get<entt::entity>(out)
            .get<Position>(out)
            .get<Velocity>(out);

        const auto str = oss.str();
        buf.assign(str.begin(), str.end());
    }
    std::cout << "[snapshot] serialized " << buf.size() << " bytes\n";

    // ── 読み込み先 registry ───────────────────────────────────────
    {
        entt::registry dst;
        std::istringstream iss(std::string(buf.begin(), buf.end()));
        auto in = [&iss](auto& v) { iss.read(reinterpret_cast<char*>(&v), sizeof(v)); };

        entt::snapshot_loader{ dst }
            .get<entt::entity>(in)
            .get<Position>(in)
            .get<Velocity>(in);

        std::cout << "[snapshot_loader] restored entities:\n";
        for (auto [entity, pos] : dst.view<Position>().each())
            std::cout << "  entity=" << (uint32_t)entity
            << " pos=(" << pos.x << "," << pos.y << ")\n";
    }
}

// =============================================================================
//  11. ユーティリティ ─ hashed_string / type_info / any / monostate
// =============================================================================
static void Demo_Utilities()
{
    PrintSection("11. Utilities");

    // ── hashed_string : コンパイル時文字列ハッシュ ───────────────
    using namespace entt::literals;
    constexpr auto posHash = "Position"_hs;
    std::cout << "[hashed_string] \"Position\" hash = " << posHash.value() << "\n";

    // ── type_info : RTTI 不要の型情報 ────────────────────────────
    auto info = entt::type_id<Position>();
    std::cout << "[type_info] name=" << info.name()
        << " hash=" << info.hash() << "\n";

    // ── any : 型消去コンテナ ─────────────────────────────────────
    // 集成体型は直接代入不可。メンバを直接 make_any に渡す
    // make_any<T>(T{...}) は T{T{...}} という入れ子aggregate初期化になりMSVCでエラー
    // → メンバ値を直接渡して T{x, y} と展開させる
    auto anyVal = entt::make_any<Position>(1.f, 2.f);
    auto* p = entt::any_cast<Position>(&anyVal);
    std::cout << "[any] stored Position: " << p->x << ", " << p->y << "\n";

    // プリミティブ型は直接代入で OK
    entt::any anyInt = 42;
    // .type() は 3.16 で deprecated → .info() を使う
    std::cout << "[any] type matches int? "
        << (anyInt.info() == entt::type_id<int>()) << "\n";

    // ── monostate : グローバル設定値ストア ───────────────────────
    entt::monostate<"gravity"_hs>{} = 9.8f;
    float g = entt::monostate<"gravity"_hs>{};
    std::cout << "[monostate] gravity = " << g << "\n";
}

// =============================================================================
//  main
// =============================================================================
int main()
{
    std::cout << "EnTT 3.16 Feature Demo\n";
    std::cout << "======================\n";

    Demo_Registry();
    Demo_View();
    Demo_Group();
    Demo_Handle();
    Demo_Dispatcher();
    Demo_Sigh();
    Demo_Emitter();
    Demo_Scheduler();
    Demo_Organizer();
    Demo_Snapshot();
    Demo_Utilities();

    std::cout << "\n=== All demos finished ===\n";
    return 0;
}
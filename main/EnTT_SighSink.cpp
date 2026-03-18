// =============================================================================
//  UE デリゲート ↔ EnTT sigh/sink 対応サンプル
// =============================================================================
#include "entt/entt.hpp"
#include <iostream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  対応表
//
//  UE                              EnTT
//  ──────────────────────────────────────────────────────────────
//  DECLARE_DELEGATE                entt::sigh（バインドを1つに制限して使う）
//  DECLARE_MULTICAST_DELEGATE      entt::sigh + entt::sink
//  BindUObject / BindRaw           sink.connect<&Func>(instance)
//  AddUObject / AddRaw             sink.connect<&Func>(instance)
//  Execute / Broadcast             signal.publish(args...)
//  IsBound / IsAlreadyBound        signal.empty() / signal.size()
//  FDelegateHandle + Remove        entt::connection + connection.release()
//  スコープ終了で自動解除          entt::scoped_connection
//  戻り値収集                      signal.collect(collector, args...)
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  受信側クラス（UE では UObject 相当）
// ─────────────────────────────────────────────────────────────────────────────
struct PlayerController
{
    void OnDamaged(int amount)
    {
        hp -= amount;
        std::cout << "[Player] Damaged " << amount << " / HP=" << hp << "\n";
    }
    int hp = 100;
};

struct HUDSystem
{
    void OnDamaged(int amount)
    {
        std::cout << "[HUD] Show damage number: " << amount << "\n";
    }
    void OnLevelUp(const std::string& msg)
    {
        std::cout << "[HUD] LevelUp: " << msg << "\n";
    }
};

struct AudioSystem
{
    void OnDamaged(int /*amount*/)
    {
        std::cout << "[Audio] Play hit sound\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  フリー関数ハンドラ
// ─────────────────────────────────────────────────────────────────────────────
static void LogDamage(int amount)
{
    std::cout << "[Log] damage=" << amount << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. DECLARE_MULTICAST_DELEGATE（最頻出）
//
//  UE:
//      DECLARE_MULTICAST_DELEGATE_OneParam(FOnDamaged, int)
//      FOnDamaged OnDamaged;
//      OnDamaged.AddUObject(this, &APlayerController::OnDamaged);
//      OnDamaged.Broadcast(30);
//
//  EnTT:
//      entt::sigh<void(int)> OnDamaged;
//      entt::sink{ OnDamaged }.connect<&PlayerController::OnDamaged>(player);
//      OnDamaged.publish(30);
// ─────────────────────────────────────────────────────────────────────────────
static void Demo_MulticastDelegate()
{
    std::cout << "\n===== 1. Multicast Delegate =====\n";

    entt::sigh<void(int)> OnDamaged;
    entt::sink sink{ OnDamaged };

    PlayerController player;
    HUDSystem        hud;
    AudioSystem      audio;

    // ── AddUObject / AddRaw 相当 ──────────────────────────────────
    sink.connect<&PlayerController::OnDamaged>(player);
    sink.connect<&HUDSystem::OnDamaged>(hud);
    sink.connect<&AudioSystem::OnDamaged>(audio);
    sink.connect<&LogDamage>();                 // フリー関数も登録可

    // ── IsBound 相当 ─────────────────────────────────────────────
    std::cout << "IsBound: " << !OnDamaged.empty()
        << " / listeners=" << OnDamaged.size() << "\n";

    // ── Broadcast 相当 ────────────────────────────────────────────
    OnDamaged.publish(30);

    // ── Remove 相当（特定リスナーだけ外す）───────────────────────
    sink.disconnect<&HUDSystem::OnDamaged>(hud);
    std::cout << "-- after disconnect HUD --\n";
    OnDamaged.publish(10);

    // ── RemoveAll 相当（全解除）──────────────────────────────────
    sink.disconnect();
    std::cout << "IsBound after RemoveAll: " << !OnDamaged.empty() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. DECLARE_DELEGATE（シングルキャスト）
//
//  UE:
//      DECLARE_DELEGATE_OneParam(FOnLevelUp, const FString&)
//      FOnLevelUp OnLevelUp;
//      OnLevelUp.BindUObject(this, &AHUDSystem::OnLevelUp);
//      OnLevelUp.ExecuteIfBound("Level 2!");
//
//  EnTT:
//      sigh 自体はマルチキャストだが、
//      最後に connect した1つだけを保持したい場合は
//      connect 前に disconnect して上書きする運用で同等になる
// ─────────────────────────────────────────────────────────────────────────────
static void Demo_SinglecastDelegate()
{
    std::cout << "\n===== 2. Singlecast Delegate =====\n";

    entt::sigh<void(const std::string&)> OnLevelUp;
    entt::sink sink{ OnLevelUp };
    HUDSystem hud;

    // Bind 相当（disconnect → connect で「上書き」）
    sink.disconnect();   // 既存バインドをクリア
    sink.connect<&HUDSystem::OnLevelUp>(hud);

    // ExecuteIfBound 相当
    if (!OnLevelUp.empty())
        OnLevelUp.publish("Level 2!");

    // Unbind 相当
    sink.disconnect();
    std::cout << "IsBound: " << !OnLevelUp.empty() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. FDelegateHandle → entt::connection
//
//  UE:
//      FDelegateHandle Handle = OnDamaged.AddUObject(...);
//      OnDamaged.Remove(Handle);
//
//  EnTT:
//      entt::connection conn = sink.connect<...>(...);
//      conn.release();
// ─────────────────────────────────────────────────────────────────────────────
static void Demo_DelegateHandle()
{
    std::cout << "\n===== 3. DelegateHandle (connection) =====\n";

    entt::sigh<void(int)> OnDamaged;
    entt::sink sink{ OnDamaged };

    PlayerController player;
    HUDSystem        hud;

    // connect の戻り値が FDelegateHandle 相当
    entt::connection playerConn = sink.connect<&PlayerController::OnDamaged>(player);
    entt::connection hudConn = sink.connect<&HUDSystem::OnDamaged>(hud);

    OnDamaged.publish(20);

    // Handle で特定リスナーだけ解除（Remove 相当）
    playerConn.release();
    std::cout << "-- after remove player handle --\n";
    OnDamaged.publish(15);

    // connection が有効かチェック（Handle.IsValid 相当）
    std::cout << "playerConn valid: " << static_cast<bool>(playerConn) << "\n";
    std::cout << "hudConn    valid: " << static_cast<bool>(hudConn) << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. scoped_connection（スコープ終了で自動解除）
//
//  UE には直接対応物はないが
//  FAutoDeleteAsyncTask / TUniquePtr での自動管理に近い感覚
//
//  EnTT の scoped_connection はデストラクタで connection.release() を呼ぶ
// ─────────────────────────────────────────────────────────────────────────────
static void Demo_ScopedConnection()
{
    std::cout << "\n===== 4. ScopedConnection =====\n";

    entt::sigh<void(int)> OnDamaged;
    entt::sink sink{ OnDamaged };
    HUDSystem hud;

    {
        // スコープ内でバインド
        entt::scoped_connection sc = sink.connect<&HUDSystem::OnDamaged>(hud);
        std::cout << "IsBound inside scope: " << !OnDamaged.empty() << "\n";
        OnDamaged.publish(5);
    }   // ← ここで sc のデストラクタが呼ばれ、自動的に disconnect される

    std::cout << "IsBound after scope: " << !OnDamaged.empty() << "\n";
    OnDamaged.publish(5);   // 誰も受け取らない
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. 戻り値付きデリゲート → collect
//
//  UE には戻り値付きのマルチキャストは存在しない（シングルキャストのみ）
//  EnTT は collect で各リスナーの戻り値を収集できる（UE より高機能）
//
//  UE:
//      DECLARE_DELEGATE_RetVal(bool, FCanJump)
//      bool bCanJump = OnCanJump.Execute();
//
//  EnTT:
//      entt::sigh<bool(void)> OnCanJump;
//      bool result = false;
//      OnCanJump.collect([&result](bool v){ result = v; });
// ─────────────────────────────────────────────────────────────────────────────
struct AbilitySystem
{
    bool CanJump() { return !isStunned; }
    bool CanAttack() { return stamina > 0; }
    bool isStunned = false;
    int  stamina = 10;
};

static void Demo_ReturnValueDelegate()
{
    std::cout << "\n===== 5. RetVal Delegate (collect) =====\n";

    entt::sigh<bool()> OnCanJump;
    entt::sink sink{ OnCanJump };
    AbilitySystem ability;

    sink.connect<&AbilitySystem::CanJump>(ability);
    sink.connect<&AbilitySystem::CanAttack>(ability);

    // 全リスナーの結果を AND で集約（全員 true なら jump 可能）
    bool canJump = true;
    OnCanJump.collect([&canJump](bool v)
        {
            canJump = canJump && v;
        });
    std::cout << "CanJump (normal): " << canJump << "\n";

    // スタン状態にして再チェック
    ability.isStunned = true;
    canJump = true;
    OnCanJump.collect([&canJump](bool v) { canJump = canJump && v; });
    std::cout << "CanJump (stunned): " << canJump << "\n";

    // --- 早期終了版（false が1つでも出たら打ち切る）---
    ability.isStunned = false;
    ability.stamina = 0;
    canJump = true;
    OnCanJump.collect([&canJump](bool v) -> bool
        {
            canJump = canJump && v;
            return !v;   // false を返すとイテレーション停止（早期 break）
        });
    std::cout << "CanJump (no stamina): " << canJump << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. ラムダをバインドする
//
//  UE:
//      OnDamaged.AddLambda([](int amount){ ... });
//
//  EnTT:
//      ラムダはテンプレート引数に渡せないため
//      dispatcher.sink<T>().connect<&free_func>() に相当しない
//      → dispatcher の enqueue/trigger 側でラムダを使うか
//      → sigh には connect(FunctionType* func, payload) の形がある
// ─────────────────────────────────────────────────────────────────────────────
static void Demo_LambdaBind()
{
    std::cout << "\n===== 6. Lambda Bind =====\n";

    entt::sigh<void(int)> OnDamaged;
    entt::sink sink{ OnDamaged };

    // ── キャプチャなしラムダ ──────────────────────────────────────
    // + 単項演算子でフリー関数ポインタに昇格させてテンプレート引数に渡す
    sink.connect < +[](int amount)
        {
            std::cout << "[Lambda] damage=" << amount << "\n";
        } > ();

    // ── キャプチャありラムダ（AddLambda相当）──────────────────────
    // sink.connect() は全オーバーロードが <Candidate> をテンプレート引数に要求する
    // → 実行時の関数ポインタ + payload を受け取るオーバーロードは存在しない
    // → キャプチャあり変数をメンバに持つ struct でラップして connect する
    struct HeavyHitChecker
    {
        int threshold = 50;
        void Check(int amount) const
        {
            if (amount >= threshold)
                std::cout << "[Struct Lambda] Heavy hit! amount=" << amount << "\n";
        }
    };
    HeavyHitChecker checker{ 50 };
    sink.connect<&HeavyHitChecker::Check>(checker);

    OnDamaged.publish(30);
    OnDamaged.publish(70);
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    Demo_MulticastDelegate();
    Demo_SinglecastDelegate();
    Demo_DelegateHandle();
    Demo_ScopedConnection();
    Demo_ReturnValueDelegate();
    Demo_LambdaBind();
    return 0;
}
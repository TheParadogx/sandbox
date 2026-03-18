// =============================================================================
//  EnTT 3.16  snapshot → バイナリファイル 保存 / 読み込みサンプル
// =============================================================================
#include "entt/entt.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  コンポーネント定義
//  ※ POD / trivially-copyable なものはそのままバイナリで書き込める
//  ※ std::string など可変長データは自前でシリアライズが必要（後述）
// ─────────────────────────────────────────────────────────────────────────────
struct Position { float x, y; };
struct Velocity { float vx, vy; };
struct Health { int hp, maxHp; };

// 可変長フィールドを持つコンポーネント（std::string）
struct Tag { std::string name; };

// ─────────────────────────────────────────────────────────────────────────────
//  OutputArchive  ─  ファイルへのバイナリ書き出しアーカイブ
//
//  EnTT は archive(value) を呼ぶだけなので
//  operator() を実装するだけで任意の出力先に対応できる
// ─────────────────────────────────────────────────────────────────────────────
class OutputArchive
{
public:
    explicit OutputArchive(const std::string& path)
        : mStream(path, std::ios::binary)
    {
        if (!mStream)
            throw std::runtime_error("OutputArchive: cannot open " + path);
    }

    // POD / trivially-copyable 用：そのままバイナリで書き込む
    template<typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>>
        operator()(const T& value)
    {
        mStream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    // std::string 用：先頭に文字数を書いてから文字列本体を書く
    void operator()(const std::string& str)
    {
        const auto len = static_cast<uint32_t>(str.size());
        mStream.write(reinterpret_cast<const char*>(&len), sizeof(len));
        mStream.write(str.data(), len);
    }

private:
    std::ofstream mStream;
};

// ─────────────────────────────────────────────────────────────────────────────
//  InputArchive  ─  ファイルからのバイナリ読み込みアーカイブ
//
//  EnTT は archive(value) を呼ぶだけなので
//  operator() の引数を非 const 参照にすれば読み込み側として機能する
// ─────────────────────────────────────────────────────────────────────────────
class InputArchive
{
public:
    explicit InputArchive(const std::string& path)
        : mStream(path, std::ios::binary)
    {
        if (!mStream)
            throw std::runtime_error("InputArchive: cannot open " + path);
    }

    // POD / trivially-copyable 用
    template<typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>>
        operator()(T& value)
    {
        mStream.read(reinterpret_cast<char*>(&value), sizeof(T));
    }

    // std::string 用
    void operator()(std::string& str)
    {
        uint32_t len{};
        mStream.read(reinterpret_cast<char*>(&len), sizeof(len));
        str.resize(len);
        mStream.read(str.data(), len);
    }

private:
    std::ifstream mStream;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Tag のシリアライズ特殊化
//
//  Tag は std::string メンバを持つため trivially-copyable ではない
//  → snapshot が archive(tag_instance) を呼んだ時に std::string 版を経由させる
//  → ここでは OutputArchive / InputArchive に operator()(const Tag&) などを
//    追加するか、Tag に serialize フリー関数を用意する方法が簡潔
//
//  ※ EnTT の snapshot は storage->get_as_tuple(entt) の各要素に
//    archive(elem) を呼ぶ。つまり archive(Tag{...}) が呼ばれる
// ─────────────────────────────────────────────────────────────────────────────

// OutputArchive に Tag 対応を追加（operator() のオーバーロード追加版）
// ※ 上の class 定義に直接追加するのが実用的だが、
//   ここでは分かりやすく継承で示す
class OutputArchiveEx : public OutputArchive
{
public:
    using OutputArchive::OutputArchive;
    using OutputArchive::operator();   // POD 版・string 版を引き継ぐ

    void operator()(const Tag& tag)
    {
        // std::string 版にディスパッチ
        (*this)(tag.name);
    }
};

class InputArchiveEx : public InputArchive
{
public:
    using InputArchive::InputArchive;
    using InputArchive::operator();

    void operator()(Tag& tag)
    {
        (*this)(tag.name);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  保存 / 読み込みの実装
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* SAVE_FILE = "world.bin";

static void Save(const entt::registry& registry)
{
    OutputArchiveEx out(SAVE_FILE);

    entt::snapshot{ registry }
        .get<entt::entity>(out)   // エンティティ一覧（freelist含む）
        .get<Position>(out)
        .get<Velocity>(out)
        .get<Health>(out)
        .get<Tag>(out);

    std::cout << "[Save] → " << SAVE_FILE << "\n";
}

static void Load(entt::registry& registry)
{
    InputArchiveEx in(SAVE_FILE);

    entt::snapshot_loader{ registry }
        .get<entt::entity>(in)    // Saveと同じ順番で読む
        .get<Position>(in)
        .get<Velocity>(in)
        .get<Health>(in)
        .get<Tag>(in)
        .orphans();               // 孤立エンティティ（コンポーネントゼロ）を削除

    std::cout << "[Load] ← " << SAVE_FILE << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main()
{
    // ── 1. 保存元 registry を構築 ─────────────────────────────────
    entt::registry src;

    auto player = src.create();
    src.emplace<Position>(player, 1.5f, 2.5f);
    src.emplace<Velocity>(player, 0.1f, 0.0f);
    src.emplace<Health>(player, 100, 100);
    src.emplace<Tag>(player, "Player");

    auto enemy1 = src.create();
    src.emplace<Position>(enemy1, 10.f, 5.f);
    src.emplace<Health>(enemy1, 30, 30);
    src.emplace<Tag>(enemy1, "Enemy_Goblin");

    auto enemy2 = src.create();
    src.emplace<Position>(enemy2, 20.f, 0.f);
    src.emplace<Health>(enemy2, 50, 50);
    src.emplace<Tag>(enemy2, "Enemy_Orc");

    // 途中で破棄してフリーリストを作る（restore できるか確認）
    src.destroy(enemy1);

    std::cout << "=== Before Save ===\n";
    for (auto [e, pos, tag] : src.view<Position, Tag>().each())
    {
        std::cout << "  [" << (uint32_t)e << "] "
            << tag.name
            << " pos=(" << pos.x << "," << pos.y << ")\n";
    }

    // ── 2. バイナリファイルへ保存 ──────────────────────────────────
    Save(src);

    // ── 3. 別の registry に読み込み ───────────────────────────────
    entt::registry dst;
    Load(dst);

    std::cout << "\n=== After Load ===\n";
    for (auto [e, pos, tag] : dst.view<Position, Tag>().each())
    {
        std::cout << "  [" << (uint32_t)e << "] "
            << tag.name
            << " pos=(" << pos.x << "," << pos.y << ")\n";
    }

    // ── 4. エンティティの同一性を確認 ─────────────────────────────
    std::cout << "\n=== Entity identity check ===\n";
    std::cout << "  src player valid: " << src.valid(player) << "\n";
    std::cout << "  dst player valid: " << dst.valid(player) << "\n";

    // health の値も確認
    if (dst.valid(player))
    {
        auto& hp = dst.get<Health>(player);
        std::cout << "  Player HP: " << hp.hp << "/" << hp.maxHp << "\n";
    }

    return 0;
}
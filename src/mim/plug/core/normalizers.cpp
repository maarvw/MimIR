#include <type_traits>

#include <mim/normalize.h>

#include <mim/plug/math/math.h>
#include <mim/plug/mem/mem.h>

#include "mim/plug/core/core.h"

namespace mim::plug::core {

namespace {

template<nat_t w>
constexpr u64 mask() {
    static_assert(w >= 1 && w <= 64);
    if constexpr (w == 64)
        return ~u64(0);
    else
        return (u64(1) << w) - 1;
}

template<nat_t w>
constexpr u64 trunc(u64 x) {
    return x & mask<w>();
}

template<nat_t w>
constexpr bool sign(u64 x) {
    static_assert(w >= 1 && w <= 64);
    return (trunc<w>(x) >> (w - 1)) & 1;
}

template<nat_t w>
constexpr s64 sext(u64 x) {
    static_assert(w >= 1 && w <= 64);
    x = trunc<w>(x);
    if constexpr (w == 64)
        return static_cast<s64>(x);
    else
        return sign<w>(x) ? static_cast<s64>(x | ~mask<w>()) : static_cast<s64>(x);
}

template<nat_t w>
constexpr bool add_nuw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u     = trunc<w>(u);
    v     = trunc<w>(v);
    u64 r = trunc<w>(u + v);
    return r < u;
}

template<nat_t w>
constexpr bool add_nsw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u       = trunc<w>(u);
    v       = trunc<w>(v);
    u64 r   = trunc<w>(u + v);
    bool su = sign<w>(u);
    bool sv = sign<w>(v);
    bool sr = sign<w>(r);
    return (su == sv) && (sr != su);
}

template<nat_t w>
constexpr bool sub_nuw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u = trunc<w>(u);
    v = trunc<w>(v);
    return u < v;
}

template<nat_t w>
constexpr bool sub_nsw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u       = trunc<w>(u);
    v       = trunc<w>(v);
    u64 r   = trunc<w>(u - v);
    bool su = sign<w>(u);
    bool sv = sign<w>(v);
    bool sr = sign<w>(r);
    return (su != sv) && (sr != su);
}

template<nat_t w>
constexpr bool mul_nuw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u = trunc<w>(u);
    v = trunc<w>(v);

    using U128 = unsigned __int128;
    U128 wide  = U128(u) * U128(v);

    if constexpr (w == 64)
        return (wide >> 64) != 0;
    else
        return (wide >> w) != 0;
}

template<nat_t w>
constexpr bool mul_nsw(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    u = trunc<w>(u);
    v = trunc<w>(v);

    using S128 = __int128_t;
    S128 a     = static_cast<S128>(sext<w>(u));
    S128 b     = static_cast<S128>(sext<w>(v));
    S128 wide  = a * b;

    S128 smin, smax;
    if constexpr (w == 64) {
        smin = static_cast<S128>(std::numeric_limits<s64>::min());
        smax = static_cast<S128>(std::numeric_limits<s64>::max());
    } else {
        smin = -(S128(1) << (w - 1));
        smax = (S128(1) << (w - 1)) - 1;
    }

    return wide < smin || wide > smax;
}

template<nat_t w>
constexpr bool shl_nuw(u64 u, unsigned k) {
    static_assert(w >= 1 && w <= 64);
    u = trunc<w>(u);
    if (k == 0) return false;
    return (u >> (w - k)) != 0;
}

template<nat_t w>
constexpr bool shl_nsw(u64 u, unsigned k) {
    static_assert(w >= 1 && w <= 64);
    u = trunc<w>(u);
    if (k == 0) return false;

    using S128 = __int128_t;
    S128 a     = static_cast<S128>(sext<w>(u));
    S128 wide  = a << k;

    S128 smin, smax;
    if constexpr (w == 64) {
        smin = static_cast<S128>(std::numeric_limits<s64>::min());
        smax = static_cast<S128>(std::numeric_limits<s64>::max());
    } else {
        smin = -(S128(1) << (w - 1));
        smax = (S128(1) << (w - 1)) - 1;
    }

    return wide < smin || wide > smax;
}

template<nat_t w>
constexpr bool sdivrem(u64 u, u64 v) {
    static_assert(w >= 1 && w <= 64);
    s64 a = sext<w>(u);
    s64 b = sext<w>(v);
    if (b == 0) return true;

    if constexpr (w == 64) {
        return a == std::numeric_limits<s64>::min() && b == -1;
    } else {
        s64 smin = -(s64(1) << (w - 1));
        return a == smin && b == -1;
    }
}

// ---------- icmp helpers ----------

template<nat_t w, icmp id>
constexpr bool fold_icmp(u64 a, u64 b) {
    static_assert(w >= 1 && w <= 64);

    const u64 u   = trunc<w>(a);
    const u64 v   = trunc<w>(b);
    const bool su = sign<w>(u);
    const bool sv = sign<w>(v);

    flags_t rel = 0;
    // clang-format off
    if (false) {}
    else if (u == v)     rel = icmp_mask & flags_t(icmp::xyglE); // equal
    else if (!su &&  sv) rel = icmp_mask & flags_t(icmp::Xygle); // plus, minus
    else if ( su && !sv) rel = icmp_mask & flags_t(icmp::xYgle); // minus, plus
    else if (u > v)      rel = icmp_mask & flags_t(icmp::xyGle); // same-sign greater
    else                 rel = icmp_mask & flags_t(icmp::xygLe); // same-sign less
    // clang-format on

    return (flags_t(id) & rel) != 0;
}

// clang-format off
static_assert(trunc<8>(0x123) == 0x23);
static_assert( sign<8>(0x80));
static_assert(!sign<8>(0x7f));
static_assert(sext<8>(0x80) == -128);
static_assert(sext<8>(0xff) == -1);
static_assert(sext<8>(0x7f) == 127);

static_assert (add_nuw<8>(255, 1));
static_assert (add_nsw<8>(127, 1));
static_assert(!add_nsw<8>(  1, 2));

static_assert(sub_nuw<8>(0, 1));
static_assert(sub_nsw<8>(0x80, 1)); // -128 - 1

static_assert(mul_nuw<8>(16, 16));
static_assert(mul_nsw<8>(20, 20));

static_assert(!shl_nuw<8>(1, 7));
static_assert( shl_nuw<8>(0x80, 1));
static_assert( shl_nsw<8>(0x40, 1));

static_assert( fold_icmp<3, icmp::e >(0b010, 0b010));
static_assert( fold_icmp<3, icmp::ne>(0b010, 0b011));
static_assert( fold_icmp<3, icmp::sg>(0b011, 0b100)); // 3 > -4
static_assert( fold_icmp<3, icmp::ul>(0b011, 0b100)); // 3 < 4
static_assert( fold_icmp<3, icmp::sl>(0b100, 0b011)); // -4 < 3
static_assert( fold_icmp<3, icmp::t >(0b001, 0b111));
static_assert(!fold_icmp<3, icmp::f >(0b001, 0b111));
// clang-format on

template<class Id, Id id, nat_t w>
Res fold(u64 a, u64 b, [[maybe_unused]] bool nsw, [[maybe_unused]] bool nuw) {
    static_assert(w >= 1 && w <= 64);

    using ST = w2s<w>;
    using UT = w2u<w>;

    const u64 u = trunc<w>(a);
    const u64 v = trunc<w>(b);

    if constexpr (std::is_same_v<Id, wrap>) {
        if constexpr (id == wrap::add) {
            if constexpr (w == 1) {
                return bool(u ^ v);
            } else {
                if (nuw && add_nuw<w>(u, v)) return {};
                if (nsw && add_nsw<w>(u, v)) return {};
                return UT(trunc<w>(u + v));
            }
        } else if constexpr (id == wrap::sub) {
            if constexpr (w == 1) {
                return bool(u ^ v);
            } else {
                if (nuw && sub_nuw<w>(u, v)) return {};
                if (nsw && sub_nsw<w>(u, v)) return {};
                return UT(trunc<w>(u - v));
            }
        } else if constexpr (id == wrap::mul) {
            if constexpr (w == 1) {
                return bool(u & v);
            } else {
                if (nuw && mul_nuw<w>(u, v)) return {};
                if (nsw && mul_nsw<w>(u, v)) return {};
                return UT(trunc<w>(u * v));
            }
        } else if constexpr (id == wrap::shl) {
            if (b >= w) return {};
            const unsigned k = static_cast<unsigned>(b);

            if constexpr (w == 1) {
                if (k != 0) return {};
                return bool(u);
            } else {
                if (nuw && shl_nuw<w>(u, k)) return {};
                if (nsw && shl_nsw<w>(u, k)) return {};
                return UT(trunc<w>(u << k));
            }
        } else {
            static_assert(false, "missing wrap subtag");
        }
    } else if constexpr (std::is_same_v<Id, shr>) {
        if (b >= w) return {};
        const unsigned k = static_cast<unsigned>(b);

        if constexpr (id == shr::a)
            if constexpr (w == 1)
                return bool(u);
            else
                return ST(static_cast<ST>(sext<w>(u) >> k));
        else if constexpr (id == shr::l)
            return UT(trunc<w>(u >> k));
        else
            static_assert(false, "missing shr subtag");
    } else if constexpr (std::is_same_v<Id, div>) {
        if (v == 0) return {};

        if constexpr (id == div::udiv)
            return UT(trunc<w>(u / v));
        else if constexpr (id == div::urem)
            return UT(trunc<w>(u % v));
        else if constexpr (id == div::sdiv) {
            if (sdivrem<w>(u, v)) return {};
            return ST(static_cast<ST>(sext<w>(u) / sext<w>(v)));
        } else if constexpr (id == div::srem) {
            if (sdivrem<w>(u, v)) return {};
            return ST(static_cast<ST>(sext<w>(u) % sext<w>(v)));
        } else {
            static_assert(false, "missing div subtag");
        }
    } else if constexpr (std::is_same_v<Id, icmp>) {
        return fold_icmp<w, id>(u, v);
    } else if constexpr (std::is_same_v<Id, extrema>) {
        if constexpr (id == extrema::sm) {
            UT uu = static_cast<UT>(u);
            UT vv = static_cast<UT>(v);
            return std::min(uu, vv);
        } else if constexpr (id == extrema::sM) {
            UT uu = static_cast<UT>(u);
            UT vv = static_cast<UT>(v);
            return std::max(uu, vv);
        } else if constexpr (id == extrema::Sm) {
            ST ss = static_cast<ST>(sext<w>(u));
            ST tt = static_cast<ST>(sext<w>(v));
            return std::min(ss, tt);
        } else if constexpr (id == extrema::SM) {
            ST ss = static_cast<ST>(sext<w>(u));
            ST tt = static_cast<ST>(sext<w>(v));
            return std::max(ss, tt);
        } else {
            static_assert(false, "missing extrema subtag");
        }
    } else {
        static_assert(false, "missing tag");
    }
}

// Note that @p a and @p b are passed by reference as fold also commutes if possible.
template<class Id, Id id>
const Def* fold(World& world, const Def* type, const Def*& a, const Def*& b, const Def* mode = {}) {
    if (a->isa<Bot>() || b->isa<Bot>()) return world.bot(type);

    if (auto la = Lit::isa(a)) {
        if (auto lb = Lit::isa(b)) {
            auto size  = Lit::as(Idx::isa(a->type()));
            auto width = Idx::size2bitwidth(size);
            bool nsw = false, nuw = false;
            if constexpr (std::is_same_v<Id, wrap>) {
                auto m = mode ? static_cast<Mode>(Lit::as(mode)) : Mode::none;
                nsw    = fe::has_flag(m, Mode::nsw);
                nuw    = fe::has_flag(m, Mode::nuw);
            }

            Res res;
            switch (width) {
#define CODE(i) \
    case i: res = fold<Id, id, i>(*la, *lb, nsw, nuw); break;
                MIM_1_8_16_32_64(CODE)
#undef CODE
                default:
                    // TODO this is super rough but at least better than just bailing out
                    res = fold<Id, id, 64>(*la, *lb, false, false);
                    if (res && !std::is_same_v<Id, icmp>) *res %= size;
            }

            return res ? world.lit(type, *res) : world.bot(type);
        }
    }

    if (::mim::is_commutative(id) && Def::greater(a, b)) std::swap(a, b);
    return nullptr;
}

template<class Id, nat_t w>
Res fold(u64 a, [[maybe_unused]] bool nsw, [[maybe_unused]] bool nuw) {
    static_assert(w >= 1 && w <= 64);

    using ST = w2s<w>;
    using UT = w2u<w>;

    const u64 u = trunc<w>(a);

    if constexpr (std::is_same_v<Id, abs>) {
        const s64 s = sext<w>(u);

        if (s >= 0) return ST(static_cast<ST>(s));

        // negative case
        if constexpr (w == 64) {
            if (s == std::numeric_limits<s64>::min()) {
                if (nsw) return {};
                return UT(u); // wrapping two's-complement abs leaves INT_MIN unchanged
            }
        } else {
            const s64 smin = -(s64(1) << (w - 1));
            if (s == smin) {
                if (nsw) return {};
                return UT(u); // same bitpattern
            }
        }

        return ST(static_cast<ST>(-s));
    } else {
        static_assert(false, "missing tag");
    }
}

template<class Id>
const Def* fold(World& world, const Def* type, const Def*& a) {
    if (a->isa<Bot>()) return world.bot(type);

    if (auto la = Lit::isa(a)) {
        auto size  = Lit::as(Idx::isa(a->type()));
        auto width = Idx::size2bitwidth(size);
        bool nsw = false, nuw = false;
        Res res;
        switch (width) {
#define CODE(i) \
    case i: res = fold<Id, i>(*la, nsw, nuw); break;
            MIM_1_8_16_32_64(CODE)
#undef CODE
            default:
                res = fold<Id, 64>(*la, false, false);
                if (res && !std::is_same_v<Id, icmp>) *res %= size;
        }

        return res ? world.lit(type, *res) : world.bot(type);
    }
    return nullptr;
}

/// Reassociates @p a and @p b according to following rules.
/// We use the following naming convention while literals are prefixed with an `l`:
/// ```
///     a    op     b
/// (x op y) op (z op w)
///
/// (1)     la    op (lz op w) -> (la op lz) op w
/// (2) (lx op y) op (lz op w) -> (lx op lz) op (y op w)
/// (3)      a    op (lz op w) ->  lz op (a op w)
/// (4) (lx op y) op      b    ->  lx op (y op b)
/// ```
template<class Id>
const Def* reassociate(Id id, World& world, [[maybe_unused]] const App* ab, const Def* a, const Def* b) {
    if (!is_associative(id)) return nullptr;

    auto xy     = Axm::isa<Id>(id, a);
    auto zw     = Axm::isa<Id>(id, b);
    auto la     = a->isa<Lit>();
    auto [x, y] = xy ? xy->template args<2>() : std::array<const Def*, 2>{nullptr, nullptr};
    auto [z, w] = zw ? zw->template args<2>() : std::array<const Def*, 2>{nullptr, nullptr};
    auto lx     = Lit::isa(x);
    auto lz     = Lit::isa(z);

    // if we reassociate, we have to forget about nsw/nuw
    auto make_op = [&world, id](const Def* a, const Def* b) { return world.call(id, Mode::none, Defs{a, b}); };

    if (la && lz) return make_op(make_op(a, z), w);             // (1)
    if (lx && lz) return make_op(make_op(x, z), make_op(y, w)); // (2)
    if (lz) return make_op(z, make_op(a, w));                   // (3)
    if (lx) return make_op(x, make_op(y, b));                   // (4)
    return nullptr;
}

template<class Id>
const Def* merge_cmps(std::array<std::array<u64, 2>, 2> tab, const Def* a, const Def* b) {
    static_assert(sizeof(sub_t) == 1, "if this ever changes, please adjust the logic below");
    static constexpr size_t num_bits = std::bit_width(Annex::num<Id>() - 1_u64);

    auto& world = a->world();
    auto a_cmp  = Axm::isa<Id>(a);
    auto b_cmp  = Axm::isa<Id>(b);

    if (a_cmp && b_cmp && a_cmp->arg() == b_cmp->arg()) {
        // push sub bits of a_cmp and b_cmp through truth table
        sub_t res   = 0;
        sub_t a_sub = a_cmp.sub();
        sub_t b_sub = b_cmp.sub();
        for (size_t i = 0; i != num_bits; ++i, res >>= 1, a_sub >>= 1, b_sub >>= 1)
            res |= tab[a_sub & 1][b_sub & 1] << 7_u8;
        res >>= (7_u8 - u8(num_bits));

        if constexpr (std::is_same_v<Id, math::cmp>)
            return world.call(math::cmp(res), /*mode*/ a_cmp->decurry()->arg(), a_cmp->arg());
        else
            return world.call(icmp(Annex::base<icmp>() | res), a_cmp->arg());
    }

    return nullptr;
}

} // namespace

template<nat id>
const Def* normalize_nat(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();
    if (is_commutative(id) && Def::greater(a, b)) std::swap(a, b);
    auto la = Lit::isa(a);
    auto lb = Lit::isa(b);

    if (la) {
        if (lb) {
            switch (id) {
                case nat::add: return world.lit_nat(*la + *lb);
                case nat::sub: return *la < *lb ? world.lit_nat_0() : world.lit_nat(*la - *lb);
                case nat::mul: return world.lit_nat(*la * *lb);
            }
        }

        if (*la == 0) {
            switch (id) {
                case nat::add: return b;
                case nat::sub: return a; // 0 - b = 0
                case nat::mul: return a; // 0 * b = 0
            }
        }

        if (*la == 1 && id == nat::mul) return b; // 1 * b = b
    }

    if (lb && *lb == 0 && id == nat::sub) return a; // a - 0 = a

    if (a == b) {
        switch (id) {
            case nat::add: return world.call(nat::mul, Defs{world.lit_nat(2), a}); // a + a = 2 * a
            case nat::sub: return world.lit_nat(0);                                // a - a = 0
            case nat::mul: break;
        }
    }

    return world.raw_app(type, callee, {a, b});
}

template<ncmp id>
const Def* normalize_ncmp(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();

    if (id == ncmp::t) return world.lit_tt();
    if (id == ncmp::f) return world.lit_ff();

    auto [a, b] = arg->projs<2>();
    if (is_commutative(id) && Def::greater(a, b)) std::swap(a, b);

    if (a == b) {
        constexpr auto eq_mask = fe::to_underlying(ncmp::e) & 0xff;
        if ((fe::to_underlying(id) & eq_mask) != 0) return world.lit_tt();
        if (id == ncmp::ne) return world.lit_ff();
    }

    if (auto la = Lit::isa(a)) {
        if (auto lb = Lit::isa(b)) {
            // clang-format off
            switch (id) {
                case ncmp:: e: return world.lit_bool(*la == *lb);
                case ncmp::ne: return world.lit_bool(*la != *lb);
                case ncmp::l : return world.lit_bool(*la <  *lb);
                case ncmp::le: return world.lit_bool(*la <= *lb);
                case ncmp::g : return world.lit_bool(*la >  *lb);
                case ncmp::ge: return world.lit_bool(*la >= *lb);
                default: fe::unreachable();
            }
            // clang-format on
        }
    }

    return world.raw_app(type, callee, {a, b});
}

template<icmp id>
const Def* normalize_icmp(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();

    if (auto result = fold<icmp, id>(world, type, a, b)) return result;
    if (id == icmp::f) return world.lit_ff();
    if (id == icmp::t) return world.lit_tt();
    if (a == b) {
        constexpr auto eq_mask = fe::to_underlying(icmp::e) & 0xff;
        if ((fe::to_underlying(id) & eq_mask) != 0) return world.lit_tt();
        if (id == icmp::ne) return world.lit_ff();
    }

    return world.raw_app(type, callee, {a, b});
}

template<extrema id>
const Def* normalize_extrema(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    if (auto result = fold<extrema, id>(world, type, a, b)) return result;
    return world.raw_app(type, callee, {a, b});
}

const Def* normalize_abs(const Def* type, const Def*, const Def* arg) {
    auto& world           = type->world();
    auto [mem, a]         = arg->projs<2>();
    auto [_, actual_type] = type->projs<2>();
    auto make_res         = [&, mem = mem](const Def* res) { return world.tuple({mem, res}); };

    if (auto result = fold<abs>(world, actual_type, a)) return make_res(result);
    return {};
}

template<bit1 id>
const Def* normalize_bit1(const Def* type, const Def* c, const Def* a) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto s      = callee->decurry()->arg();
    // TODO cope with wrap around

    if constexpr (id == bit1::id) return a;

    if (auto ls = Lit::isa(s)) {
        switch (id) {
            case bit1::f: return world.lit_idx(*ls, 0);
            case bit1::t: return world.lit_idx(*ls, *ls - 1_u64);
            case bit1::id: fe::unreachable();
            default: break;
        }

        assert(id == bit1::neg);
        if (auto la = Lit::isa(a)) return world.lit_idx_mod(*ls, ~*la);
    }

    return {};
}

template<bit2 id>
const Def* normalize_bit2(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto s      = callee->decurry()->arg();
    auto ls     = Lit::isa(s);
    // TODO cope with wrap around

    if (is_commutative(id) && Def::greater(a, b)) std::swap(a, b);

    auto tab = make_truth_table(id);
    if (auto res = merge_cmps<icmp>(tab, a, b)) return res;
    if (auto res = merge_cmps<math::cmp>(tab, a, b)) return res;

    auto la = Lit::isa(a);
    auto lb = Lit::isa(b);

    // clang-format off
    switch (id) {
        case bit2::    f: return world.lit(type, 0);
        case bit2::    t: if (ls) return world.lit(type, *ls-1_u64); break;
        case bit2::  fst: return a;
        case bit2::  snd: return b;
        case bit2:: nfst: return world.call(bit1::neg,  s, a);
        case bit2:: nsnd: return world.call(bit1::neg,  s, b);
        case bit2:: ciff: return world.call(bit2:: iff, s, Defs{b, a});
        case bit2::nciff: return world.call(bit2::niff, s, Defs{b, a});
        default:         break;
    }

    if (la && lb && ls) {
        switch (id) {
            case bit2::and_: return world.lit_idx    (*ls,   *la &  *lb);
            case bit2:: or_: return world.lit_idx    (*ls,   *la |  *lb);
            case bit2::xor_: return world.lit_idx    (*ls,   *la ^  *lb);
            case bit2::nand: return world.lit_idx_mod(*ls, ~(*la &  *lb));
            case bit2:: nor: return world.lit_idx_mod(*ls, ~(*la |  *lb));
            case bit2::nxor: return world.lit_idx_mod(*ls, ~(*la ^  *lb));
            case bit2:: iff: return world.lit_idx_mod(*ls, ~ *la |  *lb);
            case bit2::niff: return world.lit_idx    (*ls,   *la & ~*lb);
            default: fe::unreachable();
        }
    }

    // TODO rewrite using bit2
    auto unary = [&](bool x, bool y, const Def* a) -> const Def* {
        if (!x && !y) return world.lit(type, 0);
        if ( x &&  y) return ls ? world.lit(type, *ls-1_u64) : nullptr;
        if (!x &&  y) return a;
        if ( x && !y && id != bit2::xor_) return world.call(bit1::neg, s, a);
        return nullptr;
    };
    // clang-format on

    if (is_commutative(id) && a == b) {
        if (auto res = unary(tab[0][0], tab[1][1], a)) return res;
    }

    if (la) {
        if (*la == 0) {
            if (auto res = unary(tab[0][0], tab[0][1], b)) return res;
        } else if (ls && *la == *ls - 1_u64) {
            if (auto res = unary(tab[1][0], tab[1][1], b)) return res;
        }
    }

    if (lb) {
        if (*lb == 0) {
            if (auto res = unary(tab[0][0], tab[1][0], a)) return res;
        } else if (ls && *lb == *ls - 1_u64) {
            if (auto res = unary(tab[0][1], tab[1][1], a)) return res;
        }
    }

    if (auto res = reassociate<bit2>(id, world, callee, a, b)) return res;

    return world.raw_app(type, callee, {a, b});
}

const Def* normalize_idx(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    if (auto i = Lit::isa(arg)) {
        if (auto s = Lit::isa(Idx::isa(type))) {
            if (*i < *s) return world.lit_idx(*s, *i);
            if (auto m = Lit::isa(callee->arg())) return *m ? world.bot(type) : world.lit_idx_mod(*s, *i);
        }
    }

    return {};
}

const Def* normalize_idx_unsafe(const Def*, const Def*, const Def* arg) {
    auto& world = arg->world();
    if (auto i = Lit::isa(arg)) return world.lit_idx_unsafe(*i);
    return {};
}

template<shr id>
const Def* normalize_shr(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto s      = Idx::isa(arg->type());
    auto ls     = Lit::isa(s);

    if (auto result = fold<shr, id>(world, type, a, b)) return result;

    if (auto la = Lit::isa(a); la && *la == 0) {
        switch (id) {
            case shr::a: return a;
            case shr::l: return a;
        }
    }

    if (auto lb = Lit::isa(b)) {
        if (ls && *lb > *ls) return world.bot(type);

        if (*lb == 0) {
            switch (id) {
                case shr::a: return a;
                case shr::l: return a;
            }
        }
    }

    return world.raw_app(type, callee, {a, b});
}

template<wrap id>
const Def* normalize_wrap(const Def* type, const Def* c, const Def* arg) {
    auto& world = type->world();
    auto callee = c->as<App>();
    auto [a, b] = arg->projs<2>();
    auto mode   = callee->arg();
    auto s      = Idx::isa(a->type());
    auto ls     = Lit::isa(s);

    if (auto result = fold<wrap, id>(world, type, a, b)) return result;

    // clang-format off
    if (auto la = Lit::isa(a)) {
        if (*la == 0) {
            switch (id) {
                case wrap::add: return b;    // 0  + b -> b
                case wrap::sub: break;
                case wrap::mul: return a;    // 0  * b -> 0
                case wrap::shl: return a;    // 0 << b -> 0
            }
        } else if (*la == 1) {
            switch (id) {
                case wrap::add: break;
                case wrap::sub: break;
                case wrap::mul: return b;    // 1  * b -> b
                case wrap::shl: break;
            }
        }
    }

    if (auto lb = Lit::isa(b)) {
        if (*lb == 0) {
            switch (id) {
                case wrap::sub: return a;    // a  - 0 -> a
                case wrap::shl: return a;    // a >> 0 -> a
                default: fe::unreachable();
                // add, mul are commutative, the literal has been normalized to the left
            }
        }

        if (auto lm = Lit::isa(mode); lm && ls && *lm == 0 && id == wrap::sub)
            return world.call(wrap::add, mode, Defs{a, world.lit_idx_mod(*ls, ~*lb + 1_u64)}); // a - lb -> a + (~lb + 1)
        else if (id == wrap::shl && ls && *lb > *ls)
            return world.bot(type);
    }

    if (a == b) {
        switch (id) {
            case wrap::add: return world.call(wrap::mul, mode, Defs{world.lit(type, 2), a}); // a + a -> 2 * a
            case wrap::sub: return world.lit(type, 0);                                       // a - a -> 0
            case wrap::mul: break;
            case wrap::shl: break;
        }
    }
    // clang-format on

    if (auto res = reassociate<wrap>(id, world, callee, a, b)) return res;

    return world.raw_app(type, callee, {a, b});
}

template<div id>
const Def* normalize_div(const Def* full_type, const Def*, const Def* arg) {
    auto& world    = full_type->world();
    auto [mem, ab] = arg->projs<2>();
    auto [a, b]    = ab->projs<2>();
    auto [_, type] = full_type->projs<2>(); // peel off actual type
    auto make_res  = [&, mem = mem](const Def* res) { return world.tuple({mem, res}); };

    if (auto result = fold<div, id>(world, type, a, b)) return make_res(result);

    if (auto la = Lit::isa(a)) {
        if (*la == 0) return make_res(a); // 0 / b -> 0 and 0 % b -> 0
    }

    if (auto lb = Lit::isa(b)) {
        if (*lb == 0) return make_res(world.bot(type)); // a / 0 -> ⊥ and a % 0 -> ⊥

        if (*lb == 1) {
            switch (id) {
                case div::sdiv: return make_res(a);                  // a / 1 -> a
                case div::udiv: return make_res(a);                  // a / 1 -> a
                case div::srem: return make_res(world.lit(type, 0)); // a % 1 -> 0
                case div::urem: return make_res(world.lit(type, 0)); // a % 1 -> 0
            }
        }
    }

    if (a == b) {
        switch (id) {
            case div::sdiv: return make_res(world.lit(type, 1)); // a / a -> 1
            case div::udiv: return make_res(world.lit(type, 1)); // a / a -> 1
            case div::srem: return make_res(world.lit(type, 0)); // a % a -> 0
            case div::urem: return make_res(world.lit(type, 0)); // a % a -> 0
        }
    }

    return {};
}

template<conv id>
const Def* normalize_conv(const Def* dst_t, const Def*, const Def* x) {
    auto& world = dst_t->world();
    auto s_t    = x->type()->as<App>();
    auto d_t    = dst_t->as<App>();
    auto s      = s_t->arg();
    auto d      = d_t->arg();
    auto ls     = Lit::isa(s);
    auto ld     = Lit::isa(d);

    if (s_t == d_t) return x;
    if (x->isa<Bot>()) return world.bot(d_t);
    if constexpr (id == conv::s) {
        if (ls && ld && *ld < *ls) return world.call(conv::u, d, x); // just truncate - we don't care for signedness
    }

    if (auto l = Lit::isa(x); l && ls && ld) {
        if constexpr (id == conv::u) {
            if (*ld == 0) return world.lit(d_t, *l); // I64
            return world.lit(d_t, *l % *ld);
        }

        auto sw = Idx::size2bitwidth(*ls);
        auto dw = Idx::size2bitwidth(*ld);

        // clang-format off
        if (false) {}
#define M(S, D) \
        else if (S == sw && D == dw) return world.lit(d_t, w2s<D>(mim::bitcast_resize<w2s<S>>(*l)));
        M( 1,  8) M( 1, 16) M( 1, 32) M( 1, 64)
                  M( 8, 16) M( 8, 32) M( 8, 64)
                            M(16, 32) M(16, 64)
                                      M(32, 64)
        else assert(false && "TODO: conversion between different Idx sizes");
        // clang-format on
    }

    return {};
}

const Def* normalize_bitcast(const Def* dst_t, const Def*, const Def* src) {
    auto& world = dst_t->world();
    auto src_t  = src->type();

    if (src->isa<Bot>()) return world.bot(dst_t);
    if (src_t == dst_t) return src;

    if (auto other = Axm::isa<bitcast>(src))
        return other->arg()->type() == dst_t ? other->arg() : world.call<bitcast>(dst_t, other->arg());

    if (auto l = Lit::isa(src)) {
        if (dst_t->isa<Nat>()) return world.lit(dst_t, *l);
        if (Idx::isa(dst_t)) return world.lit(dst_t, *l);
    }

    return {};
}

// TODO this currently hard-codes x86_64 ABI
// TODO in contrast to C, we might want to give singleton types like 'Idx 1' or '[]' a size of 0 and simply nuke each
// and every occurance of these types in a later phase
// TODO Pi and others
template<trait id>
const Def* normalize_trait(const Def*, const Def*, const Def* type) {
    auto& world = type->world();
    if (auto ptr = Axm::isa<mem::Ptr>(type)) {
        return world.lit_nat(8);
    } else if (type->isa<Pi>()) {
        return world.lit_nat(8); // Gets lowered to function ptr
    } else if (auto size = Idx::isa(type)) {
        if (auto w = Idx::size2bitwidth(size)) return world.lit_nat(std::max(1_n, std::bit_ceil(*w) / 8_n));
    } else if (auto w = math::isa_f(type)) {
        switch (*w) {
            case 16: return world.lit_nat(2);
            case 32: return world.lit_nat(4);
            case 64: return world.lit_nat(8);
            default: fe::unreachable();
        }
    } else if (type->isa<Sigma>() || type->isa<Meet>()) {
        u64 offset = 0;
        u64 align  = 1;
        for (auto t : type->ops()) {
            auto a = Lit::isa(core::op(trait::align, t));
            auto s = Lit::isa(core::op(trait::size, t));
            if (!a || !s) return {};

            align  = std::max(align, *a);
            offset = pad(offset, *a) + *s;
        }

        offset   = pad(offset, align);
        u64 size = std::max(1_u64, offset);

        switch (id) {
            case trait::align: return world.lit_nat(align);
            case trait::size: return world.lit_nat(size);
        }
    } else if (auto arr = type->isa_imm<Arr>()) {
        auto align = op(trait::align, arr->body());
        if constexpr (id == trait::align) return align;
        auto b = op(trait::size, arr->body());
        if (b->isa<Lit>()) return world.call(nat::mul, Defs{arr->arity(), b});
    } else if (auto join = type->isa<Join>()) {
        if (auto sigma = convert(join)) return core::op(id, sigma);
    }

    return {};
}

template<pe id>
const Def* normalize_pe(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();

    if constexpr (id == pe::is_closed) {
        if (Axm::isa(pe::hlt, arg)) return world.lit_ff();
        if (arg->is_closed()) return world.lit_tt();
    }

    return {};
}

MIM_core_NORMALIZER_IMPL

} // namespace mim::plug::core

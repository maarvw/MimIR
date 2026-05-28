#include "mim/plug/tensor/phase/lower.h"

#include "mim/def.h"
#include "mim/lam.h"

#include "mim/util/types.h"

#include "mim/plug/tensor/tensor.h"

namespace mim::plug::tensor::phase {

const Def* Lower::lower_broadcast_in_dim(const App* app) {
    auto& w  = new_world();
    auto c   = rewrite(app->callee());
    auto arg = rewrite(app->arg());

    auto [s_in, s_out, input, index] = arg->projs<4>();
    auto callee                      = c->as<App>();
    auto [T, r_in, r_out]            = callee->args<3>();
    w.DLOG("lower_broadcast_in_dim");
    w.DLOG("    s_out = {} : {}", s_out, s_out->type());
    w.DLOG("    input = {} : {}", input, input->type());
    w.DLOG("    index = {} : {}", index, index->type());
    w.DLOG("    T = {} : {}", T, T->type());
    w.DLOG("    r_in = {} : {}", r_in, r_in->type());
    w.DLOG("    r_out = {} : {}", r_out, r_out->type());
    w.DLOG("    s_in = {} : {}", s_in, s_in->type());

    auto r_in_lit = r_in->isa<Lit>();
    if (!r_in_lit) return nullptr;
    auto r_in_nat  = r_in_lit->get<u64>();
    auto r_out_lit = r_out->isa<Lit>();
    if (!r_out_lit) return nullptr;
    auto r_out_nat = r_out_lit->get<u64>();

    auto s_tr_vec = DefVec(r_out_nat, [&](size_t i) {
        if (i < r_in_nat) return s_in->proj(r_in_nat, i);
        return w.lit_nat_1()->as<Def>();
    });
    auto s_tr     = w.tuple(s_tr_vec);

    std::set<u64> set_perm;
    std::map<u64, u64> map_perm;
    for (u64 i = 0; i < r_out_nat; ++i)
        set_perm.insert(i);
    for (u64 i = 0; i < r_in_nat; ++i) {
        auto idx     = index->proj(r_in_nat, i);
        auto idx_lit = Lit::isa(idx);
        if (!idx_lit) return nullptr;
        u64 idx_nat = *idx_lit;

        map_perm[idx_nat] = i;

        set_perm.erase(idx_nat);
    }
    u64 j = r_in_nat;
    for (auto i = set_perm.begin(); i != set_perm.end(); i++) {
        map_perm[*i] = j;
        j++;
    }
    auto permutation_vec = DefVec(r_out_nat, [&](size_t i) { return w.lit_idx(r_out_nat, map_perm[i]); });
    auto permutation     = w.tuple(permutation_vec);

    auto tr = w.annex<tensor::transpose>();
    tr      = w.app(tr, {T, r_out, s_tr});
    tr      = w.app(tr, {input, permutation});

    auto s_bc_vec = DefVec(r_out_nat, [&](size_t i) { return s_tr->proj(r_out_nat, map_perm.at(i)); });
    auto s_bc     = w.tuple(s_bc_vec);

    auto bc = w.annex<tensor::broadcast>();
    bc      = w.app(bc, {T, r_out});
    bc      = w.app(bc, {s_bc, s_out, tr});

    return bc;
}

const Def* Lower::lower_product_2d(const App* app) {
    auto& w  = new_world();
    auto c   = rewrite(app->callee());
    auto arg = rewrite(app->arg());

    auto [t1, t2]  = arg->projs<2>();
    auto callee    = c->as<App>();
    auto [m, k, l] = callee->args<3>();
    auto R         = callee->callee()->as<App>()->arg();
    w.DLOG("lower_product_2d");
    w.DLOG("    R = {} : {}", R, R->type());
    w.DLOG("    m = {} : {}", m, m->type());
    w.DLOG("    k = {} : {}", k, k->type());
    w.DLOG("    l = {} : {}", l, l->type());
    w.DLOG("    t1 = {} : {}", t1, t1->type());
    w.DLOG("    t2 = {} : {}", t2, t2->type());

    auto res = w.annex<tensor::product_2d_impl>();
    res      = w.app(res, R);
    res      = w.app(res, {m, k, l});
    res      = w.app(res, {t1, t2});
    return res;
}

const Def* Lower::rewrite_imm_App(const App* app) {
    if (auto bid = Axm::isa<tensor::broadcast_in_dim>(app)) {
        if (auto res = lower_broadcast_in_dim(bid)) return res;
    } else if (auto p2d = Axm::isa<tensor::product_2d>(app)) {
        if (auto res = lower_product_2d(p2d)) return res;
    }
    return Rewriter::rewrite_imm_App(app);
}

} // namespace mim::plug::tensor::phase

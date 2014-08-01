#pragma once

#include <microscopes/common/sparse_ndarray/dataview.hpp>
#include <microscopes/common/entity_state.hpp>
#include <microscopes/common/group_manager.hpp>
#include <microscopes/common/typedefs.hpp>
#include <microscopes/common/macros.hpp>
#include <microscopes/common/assert.hpp>
#include <microscopes/models/base.hpp>
#include <microscopes/io/schema.pb.h>

#include <cmath>
#include <vector>
#include <set>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <stdexcept>

namespace microscopes {
namespace irm {

class relation_definition {
public:
  relation_definition() = default;
  relation_definition(const std::vector<size_t> &domains,
                      const std::shared_ptr<models::model> &model)
    : domains_(domains), model_(model)
  {
    MICROSCOPES_DCHECK(domains.size(), "arity impossible");
    MICROSCOPES_DCHECK(model.get(), "nullptr model");
  }
  inline const std::vector<size_t> & domains() const { return domains_; }
  inline const std::shared_ptr<models::model> & model() const { return model_; }
private:
  std::vector<size_t> domains_;
  std::shared_ptr<models::model> model_;
};

class model_definition {
public:
  model_definition(const std::vector<size_t> &domains,
                   const std::vector<relation_definition> &relations);
  inline const std::vector<size_t> & domains() const { return domains_; }
  inline const std::vector<relation_definition> & relations() const { return relations_; }
private:
  std::vector<size_t> domains_;
  std::vector<relation_definition> relations_;
};

namespace detail {
struct _empty {};
typedef common::group_manager<_empty> domain;
} // namespace detail

typedef std::vector<const common::sparse_ndarray::dataview *> dataset_t;
typedef detail::domain domain;

class state {
  friend class model;
public:
  typedef domain::message_type message_type;
  typedef std::vector<size_t> tuple_t;

  struct suffstats_t {
    suffstats_t() : ident_(), count_(), ss_() {}
    common::ident_t ident_; // an identifier for outside naming
    unsigned count_; // a ref count, so we know when to remove
    std::shared_ptr<models::group> ss_;
  };

  struct relation_container_t {
    relation_container_t()
      : desc_(), hypers_(),
        suffstats_table_(), ident_table_(), ident_gen_() {}
    relation_container_t(const relation_definition &desc)
      : desc_(desc), hypers_(desc.model()->create_hypers()),
        suffstats_table_(), ident_table_(), ident_gen_() {}
    void dump(io::IrmRelation &r) const;
    relation_definition desc_;
    std::shared_ptr<models::hypers> hypers_;
    std::map<tuple_t, suffstats_t> suffstats_table_;
    std::map<common::ident_t, tuple_t> ident_table_;
    common::ident_t ident_gen_;
  };

  state(const std::vector<domain> &domains,
        const std::vector<relation_container_t> &relations);

  inline size_t
  ndomains() const
  {
    return domains_.size();
  }

  inline size_t nrelations() const { return relations_.size(); }

  inline size_t
  nentities(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].nentities();
  }

  inline size_t
  ngroups(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].ngroups();
  }

  inline const std::vector<ssize_t> &
  assignments(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].assignments();
  }

  inline std::vector<size_t>
  groups(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].groups();
  }

  inline const std::set<size_t> &
  empty_groups(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].empty_groups();
  }

  inline bool
  isactivegroup(size_t domain, size_t gid) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].isactivegroup(gid);
  }

  inline size_t
  groupsize(size_t domain, size_t gid) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].groupsize(gid);
  }

  inline common::hyperparam_bag_t
  get_domain_hp(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain id");
    return domains_[domain].get_hp();
  }

  inline void
  set_domain_hp(size_t domain, const common::hyperparam_bag_t &hp)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain id");
    domains_[domain].set_hp(hp);
  }

  inline common::value_mutator
  get_domain_hp_mutator(size_t domain, const std::string &key)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain id");
    return domains_[domain].get_hp_mutator(key);
  }

  inline common::hyperparam_bag_t
  get_relation_hp(size_t relation) const
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    return relations_[relation].hypers_->get_hp();
  }

  inline void
  set_relation_hp(size_t relation, const common::hyperparam_bag_t &hp)
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    relations_[relation].hypers_->set_hp(hp);
  }

  inline void
  set_relation_hp(size_t relation, const models::hypers &proto)
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    relations_[relation].hypers_->set_hp(proto);
  }

  inline common::value_mutator
  get_relation_hp_mutator(size_t relation, const std::string &key)
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    return relations_[relation].hypers_->get_hp_mutator(key);
  }

  inline std::vector<common::ident_t>
  suffstats_identifiers(size_t relation)
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    const auto &tab = relations_[relation].ident_table_;
    std::vector<common::ident_t> ret;
    ret.reserve(tab.size());
    for (const auto &p : tab)
      ret.push_back(p.first);
    return ret;
  }

  inline common::suffstats_bag_t
  get_suffstats(size_t relation, common::ident_t id) const
  {
    return get_suffstats_t(relation, id).ss_->get_ss();
  }

  inline bool
  get_suffstats(size_t relation, const std::vector<size_t> &gids, common::suffstats_bag_t &ss) const
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    const auto it = relations_[relation].suffstats_table_.find(gids);
    if (it == relations_[relation].suffstats_table_.end())
      return false;
    ss = it->second.ss_->get_ss();
    return true;
  }

  inline void
  set_suffstats(size_t relation, common::ident_t id, const common::suffstats_bag_t &ss)
  {
    get_suffstats_t(relation, id).ss_->set_ss(ss);
  }

  inline common::value_mutator
  get_suffstats_mutator(size_t relation, common::ident_t id, const std::string &key)
  {
    return get_suffstats_t(relation, id).ss_->get_ss_mutator(key);
  }

  inline size_t
  get_suffstats_count(size_t relation, common::ident_t id) const
  {
    return get_suffstats_t(relation, id).count_;
  }

  inline size_t
  create_group(size_t domain)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain id");
    return domains_[domain].create_group().first;
  }

  inline void
  delete_group(size_t domain, size_t gid)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain id");
    // XXX: see note in remove_value_from_feature_group()
    // XXX: our GC strategy is currently inefficient
    for (const auto &dr : domain_relations_[domain]) {
      auto &relation = relations_[dr.rel_];
      auto it = relation.suffstats_table_.begin();
      while (it != relation.suffstats_table_.end()) {
        if (it->first[dr.pos_] != gid) {
          ++it;
          continue;
        }
        MICROSCOPES_ASSERT(!it->second.count_);
        relation.ident_table_.erase(it->second.ident_);
        relation.suffstats_table_.erase(it++); // must use postfix add
      }
    }
    domains_[domain].delete_group(gid);
  }

  inline void
  add_value(size_t domain, size_t gid, size_t eid, const dataset_t &d, common::rng_t &rng)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    assert_correct_shape(d);
    add_value0(domain, gid, eid, d, rng, nullptr);
  }

  inline size_t
  remove_value(size_t domain, size_t eid, const dataset_t &d, common::rng_t &rng)
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    assert_correct_shape(d);
    return remove_value0(domain, eid, d, rng);
  }

  std::pair<std::vector<size_t>, std::vector<float>>
  score_value(size_t domain, size_t eid, const dataset_t &d, common::rng_t &rng) const
  {
    std::pair<std::vector<size_t>, std::vector<float>> ret;
    inplace_score_value(ret, domain, eid, d, rng);
    return ret;
  }

  void
  inplace_score_value(
      std::pair<std::vector<size_t>, std::vector<float>> &scores,
      size_t domain, size_t eid, const dataset_t &d, common::rng_t &rng) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    assert_correct_shape(d);
    inplace_score_value0(scores, domain, eid, d, rng);
  }

  inline float
  score_assignment(size_t domain) const
  {
    MICROSCOPES_DCHECK(domain < domains_.size(), "invalid domain");
    return domains_[domain].score_assignment();
  }

  inline float
  score_assignment() const
  {
    float score = 0.;
    for (auto &d : domains_)
      score += d.score_assignment();
    return score;
  }

  float score_likelihood(size_t relation, common::ident_t id, common::rng_t &rng) const;

  float score_likelihood(size_t relation, common::rng_t &rng) const;

  inline float
  score_likelihood(common::rng_t &rng) const
  {
    float score = 0.;
    for (size_t i = 0; i < relations_.size(); i++)
      score += score_likelihood(i, rng);
    return score;
  }

  inline void
  assert_correct_shape(const dataset_t &d) const
  {
    MICROSCOPES_DCHECK(d.size() == relations_.size(), "#s dont match");
    for (size_t i = 0; i < d.size(); i++) {
      MICROSCOPES_DCHECK(relations_[i].desc_.domains().size() == d[i]->dims(), "arity does not match");
      for (size_t j = 0; j < d[i]->dims(); j++)
        MICROSCOPES_DCHECK(domains_[relations_[i].desc_.domains()[j]].nentities() == d[i]->shape()[j], "shape does not match");
    }
  }

  // for testing
  std::vector< std::vector<size_t> >
  entity_data_positions(size_t domain, size_t eid, const dataset_t &d) const;

  std::string serialize() const;

  /**
   * initialized to an **invalid** point in the state space!
   *
   * within each domain:
   *   (A) no entities assigned
   *   (B) no groups
   *   (C) no hypers initialized
   *
   * useful primarily for testing purposes
   */
  static std::shared_ptr<state>
  unsafe_initialize(const model_definition &defn);

  static std::shared_ptr<state>
  initialize(const model_definition &defn,
             const std::vector<common::hyperparam_bag_t> &cluster_inits,
             const std::vector<common::hyperparam_bag_t> &relation_inits,
             const std::vector<std::vector<size_t>> &domain_assignments,
             const dataset_t &data,
             common::rng_t &rng);

  static std::shared_ptr<state>
  deserialize(const model_definition &defn,
              const common::serialized_t &s);

protected:
  // the *_value0 methods do no error checking

  void add_value0(size_t domain, size_t gid, size_t eid,
      const dataset_t &d, common::rng_t &rng, float *acc_score);

  size_t remove_value0(size_t domain, size_t eid,
      const dataset_t &d, common::rng_t &rng);

  void
  inplace_score_value0(
      std::pair<std::vector<size_t>, std::vector<float>> &scores,
      size_t did, size_t eid,
      const dataset_t &d, common::rng_t &rng) const;

private:

  struct rel_pos_t {
    rel_pos_t() : rel_(), pos_() {}
    rel_pos_t(size_t rel, size_t pos) : rel_(rel), pos_(pos) {}
    size_t rel_;
    size_t pos_;
  };

  void
  eids_to_gids_under_relation(
      std::vector<size_t> &gids,
      const std::vector<size_t> &eids,
      const relation_definition &desc) const;

  void
  add_value_to_feature_group(
      const std::vector<size_t> &gids,
      const common::value_accessor &value,
      relation_container_t &relation,
      common::rng_t &rng,
      float *acc_score);

  void
  remove_value_from_feature_group(
      const std::vector<size_t> &gids,
      const common::value_accessor &value,
      relation_container_t &relation,
      common::rng_t &rng);

  void
  iterate_over_entity_data(
      size_t domain,
      size_t eid,
      const dataset_t &d,
      std::function<void(size_t, const std::vector<size_t> &, const common::value_accessor &)> callback) const;

  inline suffstats_t &
  get_suffstats_t(size_t relation, common::ident_t id)
  {
    MICROSCOPES_DCHECK(relation < relations_.size(), "invalid relation id");
    auto &rel = relations_[relation];
    auto &tab = rel.ident_table_;
    auto it = tab.find(id);
    MICROSCOPES_DCHECK(it != tab.end(), "invalid ident");
    auto it1 = rel.suffstats_table_.find(it->second);
    MICROSCOPES_ASSERT(it1 != rel.suffstats_table_.end());
    return it1->second;
  }

  inline const suffstats_t &
  get_suffstats_t(size_t relation, common::ident_t id) const
  {
    return const_cast<state *>(this)->get_suffstats_t(relation, id);
  }

  // for the given domain, return a list of (relation, position) pairs
  // for each relation it is a part of
  inline std::vector<rel_pos_t>
  domain_relations(size_t d) const
  {
    std::vector<rel_pos_t> ret;
    for (size_t r = 0; r < relations_.size(); r++) {
      const auto &ds = relations_[r].desc_.domains();
      for (size_t pos = 0; pos < ds.size(); pos++)
        if (ds[pos] == d)
          ret.emplace_back(r, pos);
    }
    return ret;
  }

  std::vector<domain> domains_;
  std::vector<std::vector<rel_pos_t>> domain_relations_;
  std::vector<relation_container_t> relations_;
};

/**
 * The binds happen on a per-domain basis
 */
class model : public common::entity_based_state_object {
public:
  model(const std::shared_ptr<state> &impl,
        size_t domain,
        const std::vector<std::shared_ptr<common::sparse_ndarray::dataview>> &data)
    : impl_(impl), domain_(domain), data_(data), data_raw_()
  {
    MICROSCOPES_DCHECK(impl.get(), "nullptr impl");
    data_raw_.reserve(data_.size());
    for (auto &p : data_) {
      MICROSCOPES_DCHECK(p.get(), "nullptr dataview");
      data_raw_.push_back(p.get());
    }
    impl_->assert_correct_shape(data_raw_);
  }

  size_t nentities() const override { return impl_->nentities(domain_); }
  size_t ngroups() const override { return impl_->ngroups(domain_); }
  size_t ncomponents() const override { return impl_->nrelations(); }

  std::vector<ssize_t> assignments() const override { return impl_->assignments(domain_); }
  std::vector<size_t> groups() const override { return impl_->groups(domain_); }

  std::vector<size_t>
  empty_groups() const override
  {
    const auto &egs = impl_->empty_groups(domain_);
    return std::vector<size_t>(egs.begin(), egs.end());
  }

  size_t groupsize(size_t gid) const override { return impl_->groupsize(domain_, gid); }

  common::hyperparam_bag_t get_cluster_hp() const override { return impl_->get_domain_hp(domain_); }
  void set_cluster_hp(const common::hyperparam_bag_t &hp) override { impl_->set_domain_hp(domain_, hp); }
  common::value_mutator get_cluster_hp_mutator(const std::string &key) override { return impl_->get_domain_hp_mutator(domain_, key); }

  common::hyperparam_bag_t get_component_hp(size_t component) const override { return impl_->get_relation_hp(component); }
  void set_component_hp(size_t component, const common::hyperparam_bag_t &hp) override { impl_->set_relation_hp(component, hp); }
  void set_component_hp(size_t component, const models::hypers &proto) override { impl_->set_relation_hp(component, proto); }
  common::value_mutator get_component_hp_mutator(size_t component, const std::string &key) override { return impl_->get_relation_hp_mutator(component, key); }

  std::vector<common::ident_t>
  suffstats_identifiers(size_t component) const override
  {
    return impl_->suffstats_identifiers(component);
  }

  common::suffstats_bag_t
  get_suffstats(size_t component, common::ident_t id) const override
  {
    return impl_->get_suffstats(component, id);
  }

  void
  set_suffstats(size_t component, common::ident_t id, const common::suffstats_bag_t &ss) override
  {
    impl_->set_suffstats(component, id, ss);
  }

  common::value_mutator get_suffstats_mutator(size_t component, common::ident_t id, const std::string &key) override { return impl_->get_suffstats_mutator(component, id, key); }

  void
  add_value(size_t gid, size_t eid, common::rng_t &rng) override
  {
    impl_->add_value0(domain_, gid, eid, data_raw_, rng, nullptr);
  }

  size_t
  remove_value(size_t eid, common::rng_t &rng) override
  {
    return impl_->remove_value0(domain_, eid, data_raw_, rng);
  }

  std::pair<std::vector<size_t>, std::vector<float>>
  score_value(size_t eid, common::rng_t &rng) const override
  {
    return impl_->score_value(domain_, eid, data_raw_, rng);
  }

  void
  inplace_score_value(
      std::pair<std::vector<size_t>, std::vector<float>> &scores,
      size_t eid, common::rng_t &rng) const override
  {
    impl_->inplace_score_value0(scores, domain_, eid, data_raw_, rng);
  }

  float score_assignment() const override { return impl_->score_assignment(domain_); }

  float
  score_likelihood(size_t component, common::ident_t id, common::rng_t &rng) const override
  {
    return impl_->score_likelihood(component, id, rng);
  }

  float
  score_likelihood(size_t component, common::rng_t &rng) const override
  {
    return impl_->score_likelihood(component, rng);
  }

  size_t create_group(common::rng_t &rng) override { return impl_->create_group(domain_); }

  void delete_group(size_t gid) override { impl_->delete_group(domain_, gid); }

private:
  std::shared_ptr<state> impl_;
  size_t domain_;
  std::vector<std::shared_ptr<common::sparse_ndarray::dataview>> data_;
  std::vector<const common::sparse_ndarray::dataview *> data_raw_;
};

} // namespace irm
} // namespace microscopes

#ifndef MOD_LIB_RC_EVALCONTEXT_H
#define	MOD_LIB_RC_EVALCONTEXT_H

#include <mod/Config.h>

#include <boost/graph/adjacency_list.hpp>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mod {
class Rule;
namespace RCExp {
class Expression;
} // namespace RCExp
namespace lib {
namespace Rules {
class Real;
} // namespace Rules
namespace RC {

struct Evaluator {

	enum class VertexKind {
		Rule, Composition
	};

	enum class EdgeKind {
		First, Second, Result
	};

	struct VProp {
		VertexKind kind;
		const lib::Rules::Real *rule;
	};

	struct EProp {
		EdgeKind kind;
	};

	using GraphType = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, VProp, EProp>;
	using Vertex = boost::graph_traits<GraphType>::vertex_descriptor;
	using Edge = boost::graph_traits<GraphType>::edge_descriptor;
public:
	Evaluator(std::unordered_set<std::shared_ptr<mod::Rule> > database);
	const std::unordered_set<std::shared_ptr<mod::Rule> > &getRuleDatabase() const;
	const std::unordered_set<std::shared_ptr<mod::Rule> > &getProducts() const;
	std::unordered_set<std::shared_ptr<mod::Rule> > eval(const mod::RCExp::Expression &exp);
	void print() const;
	const GraphType &getGraph() const;
public: // evalutation interface
	// adds a rule to the database, returns true iff it was a new rule
	bool addRule(std::shared_ptr<mod::Rule> r);
	// adds a rule to the product graph list
	void giveProductStatus(std::shared_ptr<mod::Rule> r);
	// searches the database for an isomorphic rule
	// if found, the rule is deleted and the database rule is returned
	// otherwise, the rule is wrapped and returned
	// does NOT add to the database
	std::shared_ptr<mod::Rule> checkIfNew(lib::Rules::Real *rCand) const;
	// records a composition
	void suggestComposition(const lib::Rules::Real *rFirst, const lib::Rules::Real *rSecond, const lib::Rules::Real *rResult);
private: // graph interface
	Vertex getVertexFromRule(const lib::Rules::Real *r);
	Vertex getVertexFromArgs(const lib::Rules::Real *rFirst, const lib::Rules::Real *rSecond);
private:
	std::unordered_set<std::shared_ptr<mod::Rule> > database, products;
private:
	GraphType rcg;
	std::unordered_map<const lib::Rules::Real*, Vertex> ruleToVertex;
	std::map<std::pair<const lib::Rules::Real*, const lib::Rules::Real*>, Vertex> argsToVertex;
};

} // namespace RC
} // namespace lib
} // namespace mod

#endif	/* MOD_LIB_RC_EVALCONTEXT_H */
#include "RC.h"

#include <mod/Error.h>
#include <mod/Graph.h>
#include <mod/Rule.h>
#include <mod/lib/IO/IO.h>
#include <mod/lib/RC/Evaluator.h>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/variant/static_visitor.hpp>

namespace mod {

struct RCEvaluator::Pimpl {

	Pimpl(const std::unordered_set<std::shared_ptr<mod::Rule> > &database)
	: evaluator(database) { }
public:
	lib::RC::Evaluator evaluator;
};

RCEvaluator::RCEvaluator(const std::unordered_set<std::shared_ptr<mod::Rule> > &database)
: p(new Pimpl(database)) { }

RCEvaluator::~RCEvaluator() { }

const std::unordered_set<std::shared_ptr<mod::Rule> > &RCEvaluator::getRuleDatabase() const {
	return p->evaluator.getRuleDatabase();
}

const std::unordered_set<std::shared_ptr<mod::Rule> > &RCEvaluator::getProducts() const {
	return p->evaluator.getProducts();
}

std::unordered_set<std::shared_ptr<mod::Rule> > RCEvaluator::eval(const RCExp::Expression &exp) {
	return p->evaluator.eval(exp);
}

void RCEvaluator::print() const {
	p->evaluator.print();
}
//------------------------------------------------------------------------------
// Static
//------------------------------------------------------------------------------

std::shared_ptr<RCEvaluator> RCEvaluator::create(const std::unordered_set<std::shared_ptr<mod::Rule> > &database) {
	return std::shared_ptr<RCEvaluator>(new RCEvaluator(database));
}

namespace RCExp {

// Parallel
//------------------------------------------------------------------------------

Union::Union(std::vector<Expression> exps) : exps(exps) {
	if(exps.empty()) throw LogicError("A parallel RC expression must be non-empty.\n");
}

std::ostream &operator<<(std::ostream &s, const Union &par) {
	s << "{" << par.exps.front();
	for(unsigned int i = 1; i < par.exps.size(); i++)
		s << ", " << par.exps[i];
	return s << "}";
}

const std::vector<Expression> &Union::getExpressions() const {
	return exps;
}

// Bind
//------------------------------------------------------------------------------

Bind::Bind(std::shared_ptr<mod::Graph> g) : g(g) {
	assert(g);
}

std::ostream &operator<<(std::ostream &s, const Bind &b) {
	return s << "bind(" << *b.g << ")";
}

std::shared_ptr<mod::Graph> Bind::getGraph() const {
	return g;
}

// Id
//------------------------------------------------------------------------------

Id::Id(std::shared_ptr<mod::Graph> g) : g(g) {
	assert(g);
}

std::ostream &operator<<(std::ostream &s, const Id &i) {
	return s << "id(" << *i.g << ")";
}

std::shared_ptr<mod::Graph> Id::getGraph() const {
	return g;
}

// Unbind
//------------------------------------------------------------------------------

Unbind::Unbind(std::shared_ptr<mod::Graph> g) : g(g) {
	assert(g);
}

std::ostream &operator<<(std::ostream &s, const Unbind &u) {
	return s << "bind(" << *u.g << ")";
}

std::shared_ptr<mod::Graph> Unbind::getGraph() const {
	return g;
}

// Expression
//------------------------------------------------------------------------------

Expression::Expression(std::shared_ptr<mod::Rule> r) : data(r) {
	assert(r);
}

Expression::Expression(Union u) : data(u) { }

Expression::Expression(Bind bind) : data(bind) { }

Expression::Expression(Id id) : data(id) { }

Expression::Expression(Unbind unbind) : data(unbind) { }

Expression::Expression(ComposeCommon compose) : data(compose) { }

Expression::Expression(ComposeParallel compose) : data(compose) { }

Expression::Expression(ComposeSub compose) : data(compose) { }

Expression::Expression(ComposeSuper compose) : data(compose) { }

namespace {

struct Visitor : boost::static_visitor<std::ostream&> {

	Visitor(std::ostream &s) : s(s) { }

	template<typename T>
	std::ostream &operator()(const T &v) {
		return s << v;
	}

	template<typename T>
	std::ostream &operator()(const std::shared_ptr<T> &p) {
		return s << *p;
	}
public:
	std::ostream &s;
};

} // namespace 

std::ostream &operator<<(std::ostream &s, const Expression &exp) {
	return exp.applyVisitor(Visitor(s));
}

// ComposeBase
//------------------------------------------------------------------------------

ComposeBase::ComposeBase(Expression first, Expression second, bool discardNonchemical)
: first(first), second(second), discardNonchemical(discardNonchemical) { }

ComposeBase::~ComposeBase() { }

std::ostream &operator<<(std::ostream &s, const ComposeBase &compose) {
	return compose.print(s << compose.getFirst() << " *")
			<< "discardNonchemical=" << std::boolalpha << compose.discardNonchemical << ")* " << compose.getSecond();
}

const Expression &ComposeBase::getFirst() const {
	return first;
}

const Expression &ComposeBase::getSecond() const {
	return second;
}

bool ComposeBase::getDiscardNonchemical() const {
	return discardNonchemical;
}

// ComposeCommon
//------------------------------------------------------------------------------

ComposeCommon::ComposeCommon(Expression first, Expression second, bool discardNonchemical, bool maximum, bool connected)
: ComposeBase(first, second, discardNonchemical), maximum(maximum), connected(connected) { }

bool ComposeCommon::getMaxmimum() const {
	return maximum;
}

bool ComposeCommon::getConnected() const {
	return connected;
}

std::ostream &ComposeCommon::print(std::ostream &s) const {
	return s << "rcCommon(maximum=" << std::boolalpha << maximum << ", connected=" << connected << ", ";
}

// ComposeParallel
//------------------------------------------------------------------------------

ComposeParallel::ComposeParallel(Expression first, Expression second, bool discardNonchemical)
: ComposeBase(first, second, discardNonchemical) { }

std::ostream &ComposeParallel::print(std::ostream &s) const {
	return s << "rcParallel(";
}

// ComposeSub
//------------------------------------------------------------------------------

ComposeSub::ComposeSub(Expression first, Expression second, bool discardNonchemical, bool allowPartial)
: ComposeBase(first, second, discardNonchemical), allowPartial(allowPartial) { }

bool ComposeSub::getAllowPartial() const {
	return allowPartial;
}

std::ostream &ComposeSub::print(std::ostream &s) const {
	return s << "rcSub(allowPartial=" << std::boolalpha << allowPartial << ", ";
}

// ComposeSuper
//------------------------------------------------------------------------------

ComposeSuper::ComposeSuper(Expression first, Expression second, bool discardNonchemical, bool allowPartial, bool enforceConstraints)
: ComposeBase(first, second, discardNonchemical), allowPartial(allowPartial), enforceConstraints(enforceConstraints) { }

bool ComposeSuper::getAllowPartial() const {
	return allowPartial;
}

bool ComposeSuper::getEnforceConstraints() const {
	return enforceConstraints;
}

std::ostream &ComposeSuper::print(std::ostream &s) const {
	return s << "rcSuper(allowPartial=" << std::boolalpha << allowPartial << ", enforceConstraints=" << enforceConstraints << ", ";
}

} // namespace RCExp
} // namespace mod
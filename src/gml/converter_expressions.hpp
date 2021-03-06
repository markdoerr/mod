#ifndef GML_CONVERTER_EXPRESSIONS_HPP
#define GML_CONVERTER_EXPRESSIONS_HPP

#include <gml/value_type.hpp>

#include <boost/variant/get.hpp>

namespace gml {
namespace converter {

struct Unused {
	
	template<typename T>
	Unused operator=(T&&) const {
		return Unused();
	}
};

struct Parent {
};

template<typename AttrHandler>
struct Expression {
		bool checkKey(const std::string &key) const {
		return key == this->key;
	}
protected:

	Expression(const std::string &key, AttrHandler attrHandler)
	: key(key), attrHandler(attrHandler) { }

	void errorOnKey(const ast::KeyValue &kv, std::ostream &err) const {
		err << "Error at " << kv.line << ":" << kv.column << ".";
		err << " Expected key '" << this->key << "', got key '" << kv.key << "'.";
	}
	
	bool checkAndErrorOnKey(const ast::KeyValue &kv, std::ostream &err) const {
		bool res = checkKey(kv.key);
		if(!res) errorOnKey(kv, err);
		return res;
	}

	bool checkAndErrorOnType(const ast::Value &value, std::ostream &err, ValueType expected) const {
		ValueType vt = boost::apply_visitor(ValueTypeVisitor(), value.value);
		if(vt != expected) {
			err << "Error at " << value.line << ":" << value.column << ".";
			err << " Expected " << expected << " value, got " << vt << " value.";
			return false;
		}
		return true;
	}
	
	const std::string &getKey() const {
		return key;
	}
private:
	std::string key;
protected:
	AttrHandler attrHandler;
};

#define MAKE_TERMINAL(Name, Type)                                                            \
	template<typename AttrHandler>                                                             \
	struct Name : Expression<AttrHandler> {                                                    \
		using Base = Expression<AttrHandler>;                                                    \
		                                                                                         \
		Name(const std::string &key, AttrHandler attrHandler)                                    \
		: Base(key, attrHandler) { }                                                             \
		                                                                                         \
		template<typename ParentAttr>                                                            \
		bool convert(const ast::KeyValue &kv, std::ostream &err, ParentAttr &parentAttr) const { \
			bool res = Base::checkAndErrorOnKey(kv, err)                                           \
			           && Base::checkAndErrorOnType(kv.value, err, ValueType::Name);               \
			if(!res) return res;                                                                   \
			Base::attrHandler(parentAttr, boost::get<Type>(kv.value.value));                       \
			return true;                                                                           \
		}                                                                                        \
		                                                                                         \
		friend std::ostream &operator<<(std::ostream &s, const Name &expr) {                     \
			return s << #Name << "(" << expr.getKey() << ")";                                      \
		}                                                                                        \
};
MAKE_TERMINAL(Int, int);
MAKE_TERMINAL(Float, double);
MAKE_TERMINAL(String, std::string);
#undef MAKE_TERMINAL

template<typename Expr>
struct ListElement {

	ListElement(std::size_t lowerBound, std::size_t upperBound, const Expr &expr)
	: lowerBound(lowerBound), upperBound(upperBound), expr(expr) { }

	friend std::ostream &operator<<(std::ostream &s, const ListElement &el) {
		return s << "[" << el.lowerBound << ", " << el.upperBound << "](" << el.expr << ")";
	}
public:
	std::size_t lowerBound, upperBound;
	Expr expr;
};

template<std::size_t I, std::size_t N, typename ...Expr>
struct ListElementPrinter {

	static void print(std::ostream &s, const std::tuple<ListElement<Expr>...> &elems) {
		if(I > 0) s << ", ";
		s << std::get<I>(elems);
		ListElementPrinter < I + 1, N, Expr...>::print(s, elems);
	}
};

template<>
template<std::size_t N, typename ...Expr>
struct ListElementPrinter<N, N, Expr...> {

	static void print(std::ostream &s, const std::tuple<ListElement<Expr>...> &elems) { }
};

template<std::size_t I, std::size_t N, typename ...Expr>
struct ListElementHandler {

	template<typename ParentAttr>
	static bool handle(const ast::KeyValue &kv, const std::tuple<ListElement<Expr>...> &elems,
			std::ostream &err, ParentAttr &parentAttr, std::array<std::size_t, N> &count) {
		auto &elem = std::get<I>(elems);
		if(!elem.expr.checkKey(kv.key)) {
			return ListElementHandler < I + 1, N, Expr...>::handle(kv, elems, err, parentAttr, count);
		} else if(elem.expr.convert(kv, err, parentAttr)) {
			++count[I];
			if(count[I] > elem.upperBound) {
				err << "Error at " << kv.line << ":" << kv.column << ".";
				err << " Unexpected " << elem.expr << ". Already got " << elem.upperBound << " occurrences.";
				return false;
			}
			return true;
		} else {
			return false;
		}
	}
};

template<>
template<std::size_t N, typename ...Expr>
struct ListElementHandler<N, N, Expr...> {

	template<typename ParentAttr>
	static bool handle(const ast::KeyValue &kv, const std::tuple<ListElement<Expr>...> &elems,
			std::ostream &err, ParentAttr &parentAttr, std::array<std::size_t, N> &count) {
		err << "Error at " << kv.line << ":" << kv.column << ".";
		err << " Unexpected list element with key '" << kv.key << "'.";
		return false;
	}
};

template<std::size_t I, std::size_t N, typename ...Expr>
struct ListElementUpperBound {

	static bool check(const std::tuple<ListElement<Expr>...> &elems, std::ostream &err, std::array<std::size_t, N> &count) {
		auto &elem = std::get<I>(elems);
		if(count[I] < elem.lowerBound) {
			err << "Expected " << elem.lowerBound << " of " << elem.expr << ". Got only " << count[I] << ".";
			return false;
		}
		return ListElementUpperBound < I + 1, N, Expr...>::check(elems, err, count);
	}
};

template<>
template<std::size_t N, typename ...Expr>
struct ListElementUpperBound<N, N, Expr...> {

	static bool check(const std::tuple<ListElement<Expr>...> &elems, std::ostream &err, std::array<std::size_t, N> &count) {
		return true;
	}
};

template<typename Type, typename AttrHandler, typename ParentAttr>
struct ListAttrHandler {

	ListAttrHandler(const AttrHandler &attrHandler, ParentAttr &parentAttr)
	: attrHandler(attrHandler), parentAttr(parentAttr) { }

	Type &getAttr() {
		return attr;
	}

	void assignToParent() const {
		attrHandler(parentAttr, std::move(attr));
	}
private:
	Type attr;
	const AttrHandler &attrHandler;
	ParentAttr &parentAttr;
};

template<>
template<typename AttrHandler, typename ParentAttr>
struct ListAttrHandler<Unused, AttrHandler, ParentAttr> {

	ListAttrHandler(const AttrHandler&, const ParentAttr&) { }

	Unused &getAttr() {
		return unused;
	}

	void assignToParent() const { }
private:
	Unused unused;
};

template<>
template<typename AttrHandler, typename ParentAttr>
struct ListAttrHandler<Parent, AttrHandler, ParentAttr> {

	ListAttrHandler(const AttrHandler &attrHandler, ParentAttr &parentAttr)
	: attrHandler(attrHandler), parentAttr(parentAttr) { }

	ParentAttr &getAttr() {
		return parentAttr;
	}

	void assignToParent() const { }
private:
	const AttrHandler &attrHandler;
	ParentAttr &parentAttr;
};

template<typename Type, typename AttrHandler, typename ...Expr>
struct List : Expression<AttrHandler> {
	using Base = Expression<AttrHandler>;
	using Elems = std::tuple<ListElement<Expr>...>;

	List(const std::string &key, AttrHandler attrHandler, const Elems &elems)
	: Base(key, attrHandler), elems(elems) { }

	template<typename ParentAttr>
	bool convert(const ast::KeyValue &kv, std::ostream &err, ParentAttr &parentAttr) const {
		bool res = Base::checkAndErrorOnKey(kv, err)
				&& Base::checkAndErrorOnType(kv.value, err, ValueType::List);
		if(!res) return res;
		const ast::List &value = boost::get<ast::List>(kv.value.value);
		std::array < std::size_t, sizeof...(Expr) > count;
		count.fill(0);
		ListAttrHandler<Type, AttrHandler, ParentAttr> ourAttr(this->attrHandler, parentAttr);
		for(const ast::KeyValue &kv : value.list) {
			bool res = ListElementHandler < 0, sizeof...(Expr), Expr...>::handle(kv, elems, err, ourAttr.getAttr(), count);
			if(!res) return false;
		}
		res = ListElementUpperBound < 0, sizeof...(Expr), Expr...>::check(elems, err, count);
		if(!res) return false;
		ourAttr.assignToParent();
		return true;
	}

	friend std::ostream &operator<<(std::ostream &s, const List &expr) {
		s << "List(" << expr.getKey() << ")[";
		ListElementPrinter < 0, std::tuple_size<Elems>::value, Expr...>::print(s, expr.elems);
		return s << "]";
	}
private:
	Elems elems;
};

template<typename Expression>
Expression asConverter(const Expression &expr) {
	return expr;
}

} // namespace converter
} // namespace gml

#endif /* GML_CONVERTER_EXPRESSIONS_HPP */
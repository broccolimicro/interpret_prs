#include "import.h"

#include <common/standard.h>
#include <interpret_boolean/import_default.h>

namespace parse_prs {

BooleanExpressionImporter::BooleanExpressionImporter(ucs::Netlist symbols, int region, bool autoDefine) : symbols(symbols) {
	this->region.push_back(region);
	this->autoDefine = autoDefine;
}

BooleanExpressionImporter::~BooleanExpressionImporter() {
}

boolean::cover BooleanExpressionImporter::L_to_T(std::string lval, tokenizer *tokens) const {
	if (lval == "vdd") {
		return boolean::cover(1);
	} else if (lval == "gnd") {
		return boolean::cover();
	}
	if (region.back() != 0) {
		lval += "'" + std::to_string(region.back());
	}
	int uid = boolean::import_net(lval, symbols, tokens, autoDefine);
	if (uid < 0) {
		return boolean::cover();
	}
	return boolean::cover(uid, 1);
}

std::string BooleanExpressionImporter::T_to_L(boolean::cover expr, tokenizer *tokens) const {
	internal("", "sub expressions in variabe names not supported", __FILE__, __LINE__);
	return "gnd";
}

bool BooleanExpressionImporter::is_lvalue(const parse_expression::expression &syntax) const {
	return syntax.level >= expression_config::cfg->lvalueLevel;
}

std::string BooleanExpressionImporter::import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const {
	if (syntax.type < 0 or syntax.type >= (int)expression_config::cfg->literals.size() or not syntax.ptr) {
		return "gnd";
	}

	std::string type = expression_config::cfg->literals[syntax.type].first;

	if (type == "constant") {
		std::string value = syntax.ptr->get<constant_expression>().value;
		if (value == "vdd" or value == "gnd") {
			return value;
		}
		error("", "unrecognized constant value, expected 'vdd' or 'gnd'", __FILE__, __LINE__);
		return "gnd";
	} else if (type == "literal") {
		return syntax.ptr->get<literal_expression>().name;
	}
	internal("", "unsupported literal type '" + type + "'", __FILE__, __LINE__);
	return "gnd";
}

void BooleanExpressionImporter::push_properties(parse_expression::operation op, const vector<parse_expression::expression::argument> &args, tokenizer *tokens) {
	if (op.is("", "'", "", "")) { // Region
		int value = -1;
		if (args.size() == 2u) {
			std::string str = args[1].ptr->to_string("");
			value = atoi(str.c_str());
		} else {
			error("", "operator ''' expects 2 arguments, found '" + ::to_string(args.size()) + "'", __FILE__, __LINE__);
		}
		this->region.push_back(value);
	}
}

void BooleanExpressionImporter::pop_properties(parse_expression::operation op) {
	if (op.is("", "'", "", "")) { // Region
		region.pop_back();
	}
}

std::string BooleanExpressionImporter::import_modifier(parse_expression::operation op, vector<std::string> args, tokenizer *tokens) const {
	if (op.is("", "'", "", "")) { // Region
		// only affects properties
		return args[0];
	} else if (op.is("", ".", "", "")) { // Member
		std::string result = args[0];
		for (int i = 1; i < (int)args.size(); i++) {
			result += "." + args[i];
		}
		return result;
	} else if (op.is("", "[", ":", "]")) {
		std::string result = args[0];
		if (args.size() > 1u) {
			result += "[" + args[1];
			for (int i = 2; i < (int)args.size(); i++) {
				result += ":" + args[i];
			}
			result += "]";
		}
		return result;
	}
	internal("", "sub expressions in variabe names not supported", __FILE__, __LINE__);
	return "gnd";
}

boolean::cover BooleanExpressionImporter::import_unary(parse_expression::operation op, boolean::cover expr, tokenizer *tokens) const {
	if (op.is("~", "", "", "")) {
		return ~expr;
	} else if (op.is("?", "", "", "")) {
		//return expr.nulled();
		return boolean::cover();
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return expr;
}

boolean::cover BooleanExpressionImporter::import_binary(parse_expression::operation op, boolean::cover left, boolean::cover right, tokenizer *tokens) const {
	if (op.is("", "", "|", "")) {
		return left | right;
	} else if (op.is("", "", "&", "")) {
		return left & right;
	} else if (op.is("", "", "^", "")) {
		return left ^ right;
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return left;
}

boolean::cover BooleanExpressionImporter::import_modifier(parse_expression::operation op, vector<boolean::cover> args, tokenizer *tokens) const {
	if (op.is("", "'", "", "")) { // Region
		// only affects properties
		return args[0];
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return boolean::cover();
}

boolean::cover import_cover(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region, bool auto_define) {
	return BooleanExpressionImporter(nets, region, auto_define).import_expression(syntax, tokens);
}

boolean::cube import_cube(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region, bool auto_define) {
	boolean::cover result = BooleanExpressionImporter(nets, region, auto_define).import_expression(syntax, tokens);
	if (result.cubes.size() > 1) {
		if (tokens != nullptr) {
			tokens->error("expected cube, found cover", __FILE__, __LINE__);
		} else {
			error("", "expected cube, found cover", __FILE__, __LINE__);
		}
		return boolean::cube();
	} else if (result.cubes.empty()) {
		return boolean::cube(0);
	}
	return result.cubes[0];
}

BooleanCompositionImporter::BooleanCompositionImporter(ucs::Netlist symbols, int region, bool autoDefine) : symbols(symbols) {
	this->region.push_back(region);
	this->autoDefine = autoDefine;
}

BooleanCompositionImporter::~BooleanCompositionImporter() {
}

boolean::cube BooleanCompositionImporter::import_assignment(const assignment &syntax, tokenizer *tokens) const {
	BooleanExpressionImporter in(symbols, region.back(), autoDefine);

	if (syntax.operation.empty() or syntax.left.size() != 1u) {
		error("", "malformed assignment", __FILE__, __LINE__);
		return boolean::cube();
	}

	std::string lval = in.import_lvalue(syntax.left[0], tokens);
	int uid = boolean::import_net(lval, symbols, tokens, autoDefine);
	if (uid < 0) {
		return boolean::cube();
	}

 	if (syntax.operation == "+") {
		return boolean::cube(uid, 1);
	} else if (syntax.operation == "-") {
		return boolean::cube(uid, 0);
	} else if (syntax.operation == "~") {
		return boolean::cube(uid, -1);
	} else if (syntax.operation == "=") {
		std::string rval = in.import_lvalue(syntax.right, tokens);
		if (rval == "vdd") {
			return boolean::cube(uid, 1);
		} else if (rval == "gnd") {
			return boolean::cube(uid, 0);
		}
		internal("", "unsupported constant type", __FILE__, __LINE__);
		return boolean::cube();
	}
	internal("", "unsupported assignment operation", __FILE__, __LINE__);
	return boolean::cube();
}

boolean::cover BooleanCompositionImporter::import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const {
	if (syntax.type < 0 or syntax.type >= (int)expression_config::cfg->literals.size() or not syntax.ptr) {
		return boolean::cover();
	}

	return boolean::cover(import_assignment(syntax.ptr->get<assignment>(), tokens));
}

void BooleanCompositionImporter::push_properties(parse_expression::operation op, const vector<parse_expression::expression::argument> &args, tokenizer *tokens) {
	if (op.is("", "'", "", "")) { // Region
		int value = -1;
		if (args.size() == 2u) {
			std::string str = args[1].ptr->to_string("");
			value = atoi(str.c_str());
		} else {
			error("", "operator ''' expects 2 arguments, found '" + ::to_string(args.size()) + "'", __FILE__, __LINE__);
		}
		this->region.push_back(value);
	}
}

void BooleanCompositionImporter::pop_properties(parse_expression::operation op) {
	if (op.is("", "'", "", "")) { // Region
		region.pop_back();
	}
}

boolean::cover BooleanCompositionImporter::import_modifier(parse_expression::operation op, vector<boolean::cover> args, tokenizer *tokens) const {
	if (op.is("", "'", "", "")) {
		return args[0];
	}
	return parse_expression::Importer<boolean::cover>::import_modifier(op, args, tokens);
}

boolean::cover BooleanCompositionImporter::import_binary(parse_expression::operation op, boolean::cover left, boolean::cover right, tokenizer *tokens) const {
	if (op.is("", "", ":", "")) {
		return boolean::choice(left, right);
	} else if (op.is("", "", ",", "")) {
		return boolean::parallel(left, right);
	}
	internal("", "unrecognized operation", __FILE__, __LINE__);
	return left;
}

boolean::cube import_assignment(const assignment &syntax, ucs::Netlist nets, tokenizer *tokens, int region, bool auto_define) {
	return BooleanCompositionImporter(nets, region, auto_define).import_assignment(syntax, tokens);
}

boolean::cover import_choice(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region, bool auto_define) {
	return BooleanCompositionImporter(nets, region, auto_define).import_expression(syntax, tokens);
}

boolean::cube import_parallel(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region, bool auto_define) {
	boolean::cover result = BooleanCompositionImporter(nets, region, auto_define).import_expression(syntax, tokens);
	if (result.cubes.size() > 1) {
		if (tokens != nullptr) {
			tokens->error("expected cube, found cover", __FILE__, __LINE__);
		} else {
			error("", "expected cube, found cover", __FILE__, __LINE__);
		}
		return boolean::cube();
	} else if (result.cubes.empty()) {
		return boolean::cube(0);
	}
	return result.cubes[0];
}

}

namespace prs {

const bool debug = false;

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int driver, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	vector<int> to(1, drain);
	for (auto term = syntax.terms.rbegin(); term != syntax.terms.rend(); term++) {
		if (debug) cout << "handling " << term->to_string() << endl;
		prs::attributes termAttr = attr;
		if (term->size != "") {
			termAttr.size = atof(term->size.c_str());
		}
		if (term->variant != "") {
			termAttr.variant = term->variant;
		}

		// interpret the operands
		vector<int> net(1,to.back());
		if (term->sub.valid) {
			if (debug) cout << "recursing" << endl;
			net = import_guard(term->sub, pr, to.back(), driver, vdd, gnd, termAttr, default_id, tokens, auto_define);
			if (net.empty()) {
				to.pop_back();
			} else {
				to.back() = net.back();
			}
			if (debug) cout << "sources net=" << to_string(net) << " to=" << to_string(to)  << endl;
		} else if (term->ltrl.valid) {
			if (debug) cout << "literal" << endl;
			
			auto name = term->ltrl.name;
			if (default_id != 0) {
				name.region = ::to_string(default_id);
			}
			int uid = boolean::import_net(name.to_string(""), pr, tokens, auto_define);
			if (debug) cout << "created " << to_string(uid) << endl;

			if (term->ltrl.gate) {
				if (debug) cout << "adding drain=" << to.back() << " gate=" << uid << " invert=" << term->ltrl.invert << " driver=" << driver << endl;
				to.back() = pr.add_source(uid, to.back(), term->ltrl.invert ? 0 : 1, driver, termAttr);
				net.back() = to.back();
				if (debug) cout << "sources net=" << to_string(net) << " to=" << to_string(to)  << endl;
			} else {
				if (debug) cout << "adding pass " << to.back() << "<->" << uid << endl;
				pr.connect(to.back(), uid);
				pr.replace(to, to.back(), uid);
				to.pop_back();
				net.clear();
				if (debug) cout << "sources net=" << to_string(net) << " to=" << to_string(to)  << endl;
			}
		}

		if (syntax.level == parse_prs::guard::AND) {
			attr.set_internal();
		}

		// interpret the operators
		if (next(term) != syntax.terms.rend() and syntax.level == parse_prs::guard::OR) {
			to.push_back(drain);
			if (debug) cout << "adding parallel split " << to_string(to) << endl;
		} else if (next(term) != syntax.terms.rend() and syntax.level == parse_prs::guard::AND and term->pchg.valid and not net.empty()) {
			if (debug) cout << "recursing on precharge" << endl;
			int subSource = driver == 1 ? vdd : gnd;
			vector<int> otherSource = import_guard(term->pchg, pr, net.back(), subSource, vdd, gnd, prs::attributes(), default_id, tokens, auto_define);
			if (debug) cout << "othersource=" << to_string(otherSource) << endl;
			if (not otherSource.empty()) {
				pr.connect(otherSource.back(), subSource);
				pr.replace(to, otherSource.back(), subSource);
				if (debug) cout << "to=" << to_string(to)  << endl;
			}
		}
	}

	if (debug) cout << "tying up to=" << to_string(to) << endl;

	while (to.size() > 1) {
		int curr = to.back();
		to.pop_back();
		if (curr == to[0]) {
			continue;
		}

		pr.connect(curr, to[0]);
		pr.replace(to, curr, to[0]);
		if (debug) cout << "tying up to=" << to_string(to) << endl;
	}

	return to;
}

void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.weak) {
		attr.weak = true;
	}
	if (syntax.force) {
		attr.force = true;
	}
	if (syntax.pass) {
		attr.pass = true;
	}
	// TODO(edward.bingham) I need after for the minimum delay and within for maximum delay
	if (syntax.after != std::numeric_limits<uint64_t>::max()) {
		attr.delay_max = syntax.after;
	}
	if (syntax.assume.valid) {
		attr.assume = import_cover(syntax.assume, pr, tokens, default_id, auto_define);
	}

	int driver = -1;
	for (int i = 0; i < (int)syntax.action.left.size(); i++) {
		if (syntax.action.operation == "+") {
			driver = 1;
		} else if (syntax.action.operation == "-") {
			driver = 0;
		} else {
			continue;
		}

		std::string name = syntax.action.left[i].to_string();
		if (default_id != 0) {
			name += "'" + ::to_string(default_id);
		}
		int uid = boolean::import_net(name, pr, tokens, auto_define);
		pr.nets[uid].keep = syntax.keep;

		vector<int> result = import_guard(syntax.implicant, pr, uid, driver, vdd, gnd, attr, default_id, tokens, auto_define);
		if (not result.empty()) {
			pr.connect(result.back(), driver == 1 ? vdd : gnd);
		}
	}
}

void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "") {
		default_id = atoi(syntax.region.c_str());
	}

	if (gnd < 0) {
		gnd = pr.netIndex("GND", true);
	}
	if (vdd < 0) {
		vdd = pr.netIndex("Vdd", true);
	}

	pr.set_power(vdd, gnd);

	for (auto i = syntax.assume.begin(); i != syntax.assume.end(); i++) {
		if (*i == "nobackflow") {
			pr.assume_nobackflow = true;
		} else if (*i == "static") {
			pr.assume_static = true;
		}
	}

	for (auto i = syntax.require.begin(); i != syntax.require.end(); i++) {
		if (*i == "driven") {
			pr.require_driven = true;
		} else if (*i == "stable") {
			pr.require_stable = true;
		} else if (*i == "noninterfering") {
			pr.require_noninterfering = true;
		} else if (*i == "adiabatic") {
			pr.require_adiabatic = true;
		}
	}

	for (int i = 0; i < (int)syntax.rules.size(); i++) {
		import_production_rule(syntax.rules[i], pr, vdd, gnd, attr, default_id, tokens, auto_define);
	}

	for (int i = 0; i < (int)syntax.regions.size(); i++) {
		import_production_rule_set(syntax.regions[i], pr, vdd, gnd, attr, default_id, tokens, auto_define);
	}
}

}

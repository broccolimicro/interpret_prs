/*
 * import.cpp
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include "import.h"
#include <interpret_boolean/import.h>

namespace prs {

int import_guard(const parse_expression::expression &syntax, prs::production_rule_set &pr, int drain, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define, bool invert)
{
	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	if (syntax.level >= (int)syntax.precedence.size()) {
		if (tokens != NULL) {
			tokens->load(&syntax);
			tokens->error("unrecognized operation", __FILE__, __LINE__);
		} else {
			error(syntax.to_string(), "unrecognized operation", __FILE__, __LINE__);
		}
		return drain;
	}

	bool err = false;
	bool subInvert = invert;
	cout << "importing " << syntax.to_string() << endl;
	if (syntax.precedence[syntax.level].type == parse_expression::operation_set::left_unary) {
		for (int j = (int)syntax.operations.size()-1; j >= 0; j--) {
			if (syntax.operations[j] == "~") {
				subInvert = not subInvert;
			} else {
				err = true;
			}
		}
	} else if (syntax.precedence[syntax.level].type == parse_expression::operation_set::right_unary and not syntax.operations.empty()) {
		err = true;
	}

	if (err) {
		if (tokens != NULL) {
			tokens->load(&syntax);
			tokens->error("unsupported operation", __FILE__, __LINE__);
		} else {
			error(syntax.to_string(), "unsupported operation", __FILE__, __LINE__);
		}
	}

	vector<int> to(1, drain);
	for (int i = (int)syntax.arguments.size()-1; i >= 0; i--)
	{
		// interpret the operands
		if (syntax.arguments[i].sub.valid) {
			cout << "found sub expression " << endl;
			to.back() = import_guard(syntax.arguments[i].sub, pr, to.back(), variables, default_id, tokens, auto_define, subInvert);
		} else if (syntax.arguments[i].literal.valid) {
			cout << "found literal " << syntax.arguments[i].to_string() << endl;
			vector<int> v = define_variables(syntax.arguments[i].literal, variables, default_id, tokens, auto_define, auto_define);
			for (int j = 0; j < (int)v.size(); j++) {
				if (v[j] >= 0) {
					cout << "adding source " << v[j] << " " << to.back() << " " << subInvert << endl;
					to.back() = pr.add_source(v[j], to.back(), subInvert ? 0 : 1, false, false, 0);
				}
			}
		}

		// interpret the operators
		if (i != 0 and syntax.operations[i-1] == "|") {
			to.push_back(drain);
		} else if (i != 0 and syntax.operations[i-1] != "&") {
			err = true;
		}

		if (err) {
			if (tokens != NULL) {
				tokens->load(&syntax);
				tokens->error("unsupported operation", __FILE__, __LINE__);
			} else {
				error(syntax.to_string(), "unsupported operation", __FILE__, __LINE__);
			}
		}
	}

	for (int i = (int)to.size()-1; i >= 1; i--) {
		pr.connect(to[i], to[0]);
	}

	cout << "done" << endl;

	return to[0];
}

void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	/*if (syntax.assume.valid) {
		result.assume = import_cover(syntax.assume, variables, default_id, tokens, auto_define);
	}*/

	int source = -1;
	for (int i = 0; i < (int)syntax.action.names.size(); i++) {
		if (syntax.action.operation == "+") {
			source = vdd;
		} else if (syntax.action.operation == "-") {
			source = gnd;
		} else {
			continue;
		}

		int action_id = default_id;
		if (syntax.action.region != "") {
			action_id = atoi(syntax.action.region.c_str());
		}

		cout << "reading production rule " << syntax.to_string() << endl;
		cout << "source=" << source << endl;	
		vector<int> v = define_variables(syntax.action.names[i], variables, action_id, tokens, auto_define, auto_define);
		for (int j = 0; j < (int)v.size(); j++) {
			pr.connect(import_guard(syntax.implicant, pr, v[j], variables, default_id, tokens, auto_define, false), source);
		}
	}
}

void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	if (gnd < 0) {
		gnd = variables.find(ucs::variable("GND"));
	}
	if (gnd < 0) {
		gnd = variables.define(ucs::variable("GND"));
	}
	if (vdd < 0) {
		vdd = variables.find(ucs::variable("Vdd"));
	}
	if (vdd < 0) {
		vdd = variables.define(ucs::variable("Vdd"));
	}

	cout << "vdd=" << vdd << endl;
	cout << "gnd=" << gnd << endl;

	if (pr.nets.size() < variables.nodes.size()) {
		pr.nets.resize(variables.nodes.size());
	}

	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	for (int i = 0; i < (int)syntax.rules.size(); i++) {
		import_production_rule(syntax.rules[i], pr, vdd, gnd, variables, default_id, tokens, auto_define);
	}

	for (int i = 0; i < (int)syntax.regions.size(); i++) {
		import_production_rule_set(syntax.regions[i], pr, vdd, gnd, variables, default_id, tokens, auto_define);
	}
}

}

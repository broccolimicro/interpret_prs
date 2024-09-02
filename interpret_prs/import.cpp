/*
 * import.cpp
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include "import.h"
#include <interpret_boolean/import.h>

namespace prs {

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int source, int vdd, int gnd, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	cout << "importing " << syntax.to_string() << endl;

	vector<int> to(1, drain);
	for (int i = (int)syntax.terms.size()-1; i >= 0; i--) {
		// interpret the operands
		vector<int> net(1,to.back());
		if (syntax.terms[i].sub.valid) {
			cout << "found sub expression " << endl;
			net = import_guard(syntax.terms[i].sub, pr, to.back(), source, vdd, gnd, variables, default_id, tokens, auto_define);
			if (net.empty()) {
				to.pop_back();
			} else {
				to.back() = net.back();
			}
		} else if (syntax.terms[i].ltrl.valid) {
			cout << "found literal " << syntax.terms[i].to_string() << endl;
			vector<int> v = define_variables(syntax.terms[i].ltrl.name, variables, default_id, tokens, auto_define, auto_define);
			if (v.size() != 1 or v[0] < 0) {
				if (tokens != NULL) {
					tokens->load(&syntax.terms[i].ltrl.name);
					tokens->error("literals must be a wire", __FILE__, __LINE__);
				} else {
					error(syntax.to_string(), "literals must be a wire", __FILE__, __LINE__);
				}
			}

			cout << "adding source " << v.back() << " " << to.back() << " " << endl;
			if (syntax.terms[i].ltrl.gate) {
				to.back() = pr.add_source(v.back(), to.back(), syntax.terms[i].ltrl.invert ? 0 : 1, false, false, 0);
				net.back() = to.back();
			} else {
				pr.connect(to.back(), v.back());
				pr.replace(to, to.back(), v.back());
				to.pop_back();
				net.clear();
			}
		}

		// interpret the operators
		if (i != 0 and syntax.level == parse_prs::guard::OR) {
			to.push_back(drain);
		} else if (i != 0 and syntax.level == parse_prs::guard::AND and syntax.terms[i].pchg.valid and not net.empty()) {
			int subSource = source == vdd ? gnd : vdd;
			vector<int> otherSource = import_guard(syntax.terms[i].pchg, pr, net.back(), subSource, vdd, gnd, variables, default_id, tokens, auto_define);
			if (not otherSource.empty()) {
				pr.connect(otherSource.back(), subSource);
				pr.replace(to, otherSource.back(), subSource);
			}
		}
	}

	while (to.size() > 1) {
		int curr = to.back();
		to.pop_back();
		if (curr == to[0]) {
			continue;
		}

		pr.connect(curr, to[0]);
		pr.replace(to, curr, to[0]);
	}

	cout << "done" << endl;

	return to;
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
		if (v.size() != 1 or v[0] < 0) {
			if (tokens != NULL) {
				tokens->load(&syntax.action.names[i]);
				tokens->error("literals must be a wire", __FILE__, __LINE__);
			} else {
				error(syntax.to_string(), "literals must be a wire", __FILE__, __LINE__);
			}
		}

		vector<int> result = import_guard(syntax.implicant, pr, v[0], source, vdd, gnd, variables, default_id, tokens, auto_define);
		if (not result.empty()) {
			pr.connect(result.back(), source);
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

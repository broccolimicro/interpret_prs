#include "import.h"

#include <interpret_ucs/import.h>
#include <interpret_boolean/import.h>

namespace prs {

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int driver, int vdd, int gnd, boolean::cover assume, attributes attr, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	vector<int> to(1, drain);
	for (int i = (int)syntax.terms.size()-1; i >= 0; i--) {
		attributes termAttr = attr;
		if (syntax.terms[i].width != "") {
			termAttr.width = atof(syntax.terms[i].width.c_str());
		}
		if (syntax.terms[i].length != "") {
			termAttr.length = atof(syntax.terms[i].width.c_str());
		}
		if (syntax.terms[i].variant != "") {
			termAttr.variant = syntax.terms[i].variant;
		}

		// interpret the operands
		vector<int> net(1,to.back());
		if (syntax.terms[i].sub.valid) {
			net = import_guard(syntax.terms[i].sub, pr, to.back(), driver, vdd, gnd, assume, termAttr, variables, default_id, tokens, auto_define);
			if (net.empty()) {
				to.pop_back();
			} else {
				to.back() = net.back();
			}
		} else if (syntax.terms[i].ltrl.valid) {
			vector<int> v = define_variables(syntax.terms[i].ltrl.name, variables, default_id, tokens, auto_define, auto_define);
			if (v.size() != 1 or v[0] < 0) {
				if (tokens != NULL) {
					tokens->load(&syntax.terms[i].ltrl.name);
					tokens->error("literals must be a wire", __FILE__, __LINE__);
				} else {
					error(syntax.to_string(), "literals must be a wire", __FILE__, __LINE__);
				}
			}
			pr.create(v[0]);
			for (int i = 0; i < (int)variables.nodes.size(); i++) {
				if (v[0] != i and variables.nodes[i].name == variables.nodes[v[0]].name) {
					pr.connect_remote(v[0], i);
				}
			}

			if (syntax.terms[i].ltrl.gate) {
				to.back() = pr.add_source(v.back(), to.back(), syntax.terms[i].ltrl.invert ? 0 : 1, driver, assume, termAttr);
				net.back() = to.back();
			} else {
				pr.connect(to.back(), v.back());
				pr.replace(to, to.back(), v.back());
				to.pop_back();
				net.clear();
			}
		}

		if (syntax.level == parse_prs::guard::AND) {
			assume = 1;
		}

		// interpret the operators
		if (i != 0 and syntax.level == parse_prs::guard::OR) {
			to.push_back(drain);
		} else if (i != 0 and syntax.level == parse_prs::guard::AND and syntax.terms[i].pchg.valid and not net.empty()) {
			int subSource = driver == 1 ? vdd : gnd;
			vector<int> otherSource = import_guard(syntax.terms[i].pchg, pr, net.back(), subSource, vdd, gnd, assume, attributes(), variables, default_id, tokens, auto_define);
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

	return to;
}

void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.weak) {
		attr.weak = true;
	}
	if (syntax.pass) {
		attr.pass = true;
	}
	if (syntax.after != std::numeric_limits<uint64_t>::max()) {
		attr.delay_max = syntax.after;
	}

	boolean::cover assume=1;
	if (syntax.assume.valid) {
		assume = import_cover(syntax.assume, variables, default_id, tokens, auto_define);
	}

	int driver = -1;
	for (int i = 0; i < (int)syntax.action.names.size(); i++) {
		if (syntax.action.operation == "+") {
			driver = 1;
		} else if (syntax.action.operation == "-") {
			driver = 0;
		} else {
			continue;
		}

		int action_id = default_id;
		if (syntax.action.region != "") {
			action_id = atoi(syntax.action.region.c_str());
		}

		vector<int> v = define_variables(syntax.action.names[i], variables, action_id, tokens, auto_define, auto_define);
		if (v.size() != 1 or v[0] < 0) {
			if (tokens != NULL) {
				tokens->load(&syntax.action.names[i]);
				tokens->error("literals must be a wire", __FILE__, __LINE__);
			} else {
				error(syntax.to_string(), "literals must be a wire", __FILE__, __LINE__);
			}
		}
		pr.create(v[0], syntax.keep);
		for (int i = 0; i < (int)variables.nodes.size(); i++) {
			if (v[0] != i and variables.nodes[i].name == variables.nodes[v[0]].name) {
				pr.connect_remote(v[0], i);
			}
		}

		vector<int> result = import_guard(syntax.implicant, pr, v[0], driver, vdd, gnd, assume, attr, variables, default_id, tokens, auto_define);
		if (not result.empty()) {
			pr.connect(result.back(), driver == 1 ? vdd : gnd);
		}
	}
}

void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "") {
		default_id = atoi(syntax.region.c_str());
	}

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

	if (pr.nets.size() < variables.nodes.size()) {
		pr.create(variables.nodes.size()-1);
	}
	pr.set_power(vdd, gnd);

	for (int i = 0; i < (int)syntax.rules.size(); i++) {
		import_production_rule(syntax.rules[i], pr, vdd, gnd, attr, variables, default_id, tokens, auto_define);
	}

	for (int i = 0; i < (int)syntax.regions.size(); i++) {
		import_production_rule_set(syntax.regions[i], pr, vdd, gnd, attr, variables, default_id, tokens, auto_define);
	}
}

}

#include "import.h"

#include <common/standard.h>

#include <interpret_boolean/import.h>

namespace prs {

const bool debug = false;

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int driver, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	vector<int> to(1, drain);
	for (auto term = syntax.terms.rbegin(); term != syntax.terms.rend(); term++) {
		if (debug) cout << "handling " << term->to_string() << endl;
		attributes termAttr = attr;
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
			int uid = boolean::import_net(term->ltrl.name, pr, default_id, tokens, auto_define);
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
			vector<int> otherSource = import_guard(term->pchg, pr, net.back(), subSource, vdd, gnd, attributes(), default_id, tokens, auto_define);
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

void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define)
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
		attr.assume = boolean::import_cover(syntax.assume, pr, default_id, tokens, auto_define);
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

		int uid = boolean::import_net(syntax.action.names[i], pr, action_id, tokens, auto_define);
		pr.nets[uid].keep = syntax.keep;

		vector<int> result = import_guard(syntax.implicant, pr, uid, driver, vdd, gnd, attr, default_id, tokens, auto_define);
		if (not result.empty()) {
			pr.connect(result.back(), driver == 1 ? vdd : gnd);
		}
	}
}

void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define)
{
	if (syntax.region != "") {
		default_id = atoi(syntax.region.c_str());
	}

	if (gnd < 0) {
		gnd = pr.netIndex("GND", 0, true);
	}
	if (vdd < 0) {
		vdd = pr.netIndex("Vdd", 0, true);
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

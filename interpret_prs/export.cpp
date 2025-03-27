#include "export.h"
#include <common/standard.h>
#include <common/text.h>

#include <interpret_boolean/export.h>
#include <parse_dot/node_id.h>

namespace prs {

const bool debug = true;

globals::globals() {
	vdd = -1;
	gnd = -1;
}

globals::globals(const prs::production_rule_set &pr) {
	if (pr.pwr.empty()) {
		for (int i = 0; i < (int)pr.nets.size(); i++) {
			// Identify power nets based on naming conventions
			// VDD, GND, and VSS are common power net names
			string lname = lower(pr.nets[i].name);
			if (lname.find("weak") == string::npos) {
				if (lname.find("vdd") != string::npos) {
					vdd = i;
				} else if (lname.find("gnd") != string::npos
					or lname.find("vss") != string::npos) {
					gnd = i;
				}
			}
		}
	} else {
		vdd = pr.pwr[0][1];
		gnd = pr.pwr[0][0];
	}
}

globals::~globals() {
}

globals::operator bool() {
	return gnd >= 0 and vdd >= 0;
}

parse_prs::guard export_guard(const prs::production_rule_set &pr, int drain, int value, attributes attr, globals g, vector<int> *next, vector<int> *covered) {
	struct walker {
		int drain;
		vector<parse_prs::guard*> stack;
		attributes attr;
	};

	if (not g) {
		g = globals(pr);
	}

	parse_prs::guard result;
	result.valid = true;
	result.level = parse_prs::guard::AND;
	vector<walker> stack(1, {drain, vector<parse_prs::guard*>(1, &result), attr});
	
	// net -> elements in stack at net
	map<int, vector<vector<int> > > merge;
	bool step = true;
	while (not stack.empty() and step) {
		step = false;
		// 1. look for opportunities to merge expressions. There need to
		// be as many iterators on a device as there are items in the
		// drain's "sourceOf" list

		for (int i = 0; i < (int)stack.size(); i++) {
			auto pos = merge.insert(pair<int, vector<vector<int> > >(stack[i].drain, vector<vector<int> >())).first;
			bool found = false;
			for (int j = 0; j < (int)pos->second.size() and not found; j++) {
				int idx = pos->second[j].back();
				if (stack[i].stack[stack[i].stack.size()-2] == stack[idx].stack[stack[idx].stack.size()-2]) {
					pos->second[j].push_back(i);
					found = true;
				}
			}
			if (not found) {
				pos->second.push_back(vector<int>(1, i));
			}
		}

		// 2. for each item in the stack, step forward if we pass the
		// test in step 1.
		vector<int> toErase;
		for (auto i = merge.begin(); i != merge.end(); i++) {
			for (int k = 0; k < (int)i->second.size(); k++) {
				if (i->first == drain or (int)i->second[k].size() >= pr.sources(i->first, value)) {
					// Do the merge
					if (i->second[k].size() > 1) {
						toErase.insert(toErase.end(), i->second[k].begin()+1, i->second[k].end());
						stack[i->second[k][0]].stack.pop_back();
					}

					int idx = i->second[k][0];
					int net = i->first;
					if (net >= (int)pr.nets.size()) {
						printf("error: net out of bounds\n");
						return result;
					}

					// Handle precharge
					if (net != drain and not stack[idx].stack.back()->terms.empty() and pr.drains(net, 1-value) != 0) {
						stack[idx].stack.back()->terms.begin()->pchg = export_guard(pr, net, 1-value, attr, g, next, covered);
					}

					// Do the split
					int drains = pr.drains(net, value);
					int drains0 = pr.drains(net, value, stack[idx].attr);
					if (net != drain and drains0 != drains) {
						// This is a source (either a pass transistor or power)
						if (debug) cout << "exiting at node " << net << endl;
						if (stack[idx].drain != g.vdd and stack[idx].drain != g.gnd) {
							parse_prs::term arg(parse_prs::literal(boolean::export_net(stack[idx].drain, pr), false, false));
							if (debug) cout << "adding term " << arg.to_string() << endl;
							if (not stack[idx].stack.empty()) {
								stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), arg);
								if (next != nullptr and (covered == nullptr or find(covered->begin(), covered->end(), stack[idx].drain) == covered->end())) {
									next->push_back(stack[idx].drain);
								}
							}
						}
						toErase.push_back(idx);
					} else if (drains > 1) {
						// This is a split
						if (debug) cout << drains << " drains at " << net << endl;
						stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), parse_prs::term(parse_prs::guard()));
						parse_prs::guard *sub = &stack[idx].stack.back()->terms.begin()->sub;
						sub->valid = true;
						sub->level = parse_prs::guard::OR;
						//sub->terms.reserve(drains*2);
						for (int j = (int)pr.nets[net].drainOf[value].size()-1; j >= 0; j--) {
							auto dev = pr.devs.begin()+pr.nets[net].drainOf[value][j];
							if (debug) cout << pr.nets[net].drainOf[value][j] << endl;
							if (dev->drain != net or dev->attr != stack[idx].attr) {
								if (debug) {
									cout << "skipping " << dev->drain << "!=" << net << endl;
									cout << "stack: " << stack[idx].attr.weak << " " << stack[idx].attr.force << " " << stack[idx].attr.pass << " " << stack[idx].attr.delay_max << " " << stack[idx].attr.assume << endl;
									cout << "dev: " << dev->attr.weak << " " << dev->attr.force << " " << dev->attr.pass << " " << dev->attr.delay_max << " " << dev->attr.assume << endl;
								}
								continue;
							}

							sub->terms.push_back(parse_prs::term(parse_prs::guard()));
							parse_prs::guard *subj = &sub->terms.back().sub;
							subj->valid = true;
							subj->level = parse_prs::guard::AND;

							// add this literal
							parse_prs::term arg(parse_prs::literal(boolean::export_net(dev->gate, pr), dev->threshold == 0));
							if (dev->attr.size > 0.0) {
								arg.size = to_minstring(dev->attr.size);
								if (dev->attr.variant != "" and dev->attr.variant != "svt") {
									arg.variant = dev->attr.variant;
								}
							}

							subj->terms.insert(subj->terms.begin(), arg);

							if (covered != nullptr) {
								auto pos = lower_bound(covered->begin(), covered->end(), stack[idx].drain);
								if (pos == covered->end() or *pos != stack[idx].drain) {
									covered->insert(pos, stack[idx].drain);
								}
							}
							stack.push_back(stack[idx]);
							stack.back().stack.push_back(subj);
							stack.back().drain = dev->source;
							stack.back().attr.set_internal();
						}
						toErase.push_back(idx);
					} else if (drains != 0) {
						if (debug) cout << "one drain " << net << endl;
						auto dev = pr.devs.begin()+pr.nets[net].drainOf[value][0];
						for (auto di = pr.nets[net].drainOf[value].begin(); di != pr.nets[net].drainOf[value].end(); di++) {
							if (debug) cout << "dev=" << *di << "/" << pr.devs.size() << endl;
							if (pr.devs[*di].drain == net and pr.devs[*di].attr == attr) {
								dev = pr.devs.begin()+*di;
								if (debug) cout << "found" << endl;
								break;
							}
						}

						// add this literal
						parse_prs::term arg(parse_prs::literal(boolean::export_net(dev->gate, pr), dev->threshold == 0));
						if (dev->attr.size > 0.0) {
							arg.size = to_minstring(dev->attr.size);
							if (dev->attr.variant != "" and dev->attr.variant != "svt") {
								arg.variant = dev->attr.variant;
							}
						}

						if (covered != nullptr) {
							auto pos = lower_bound(covered->begin(), covered->end(), stack[idx].drain);
							if (pos == covered->end() or *pos != stack[idx].drain) {
								covered->insert(pos, stack[idx].drain);
							}
						}

						stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), arg);
						stack[idx].drain = dev->source;
						stack[idx].attr.set_internal();
					} else {
						if (debug) cout << "exiting node 2 " << net << endl;
						if (stack[idx].drain != g.vdd and stack[idx].drain != g.gnd) {
							parse_prs::term arg(parse_prs::literal(boolean::export_net(stack[idx].drain, pr), false, false));
							if (debug) cout << "adding term " << arg.to_string() << endl;
							if (not stack[idx].stack.empty()) {
								stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), arg);
								if (next != nullptr and (covered == nullptr or find(covered->begin(), covered->end(), stack[idx].drain) == covered->end())) {
									next->push_back(stack[idx].drain);
								}
							}
						}
						toErase.push_back(idx);
					}
					
					step = true;
				}
			}
		}

		// cleanup the stack
		merge.clear();
		sort(toErase.begin(), toErase.end());
		toErase.erase(unique(toErase.begin(), toErase.end()), toErase.end());
		for (int i = (int)toErase.size()-1; i >= 0; i--) {
			stack[toErase[i]].stack.clear();
			stack.erase(stack.begin() + toErase[i]);
		}
	}

	//   a. if none of the items in the stack pass the test, then we
	//   stop here and those nodes become virtual nodes.
	for (auto i = stack.begin(); i != stack.end(); i++) {
		if (debug) cout << "capping " << i->drain << endl;
		if (i->drain != g.vdd and i->drain != g.gnd) {
			parse_prs::term arg(parse_prs::literal(boolean::export_net(i->drain, pr), false, false));
			if (debug) cout << "adding term " << arg.to_string() << endl;
			i->stack.back()->terms.insert(i->stack.back()->terms.begin(), arg);
		}

		if (next != nullptr and (covered == nullptr or find(covered->begin(), covered->end(), i->drain) == covered->end())) {
			next->push_back(i->drain);
		}
	}
	if (next != nullptr) {
		sort(next->begin(), next->end());
		next->erase(unique(next->begin(), next->end()), next->end());
	}
	stack.clear();

	// Clean up final result
	vector<parse_prs::guard*> walk;
	walk.push_back(&result);
	while (not walk.empty()) {
		auto curr = walk.back();
		walk.pop_back();

		for (auto term = curr->terms.begin(); term != curr->terms.end();) {
			if ((not term->sub.valid and not term->ltrl.valid)
				or (term->sub.valid and term->sub.terms.empty())) {
				term = curr->terms.erase(term);
			} else {
				term++;
			}
		}

		while (curr->terms.size() == 1 and curr->terms.begin()->sub.valid) {
			// assigning *curr = curr->terms[0].sub; assigns curr->terms first which destroys the thing we're assigning from.
			parse_prs::guard tmp = curr->terms.begin()->sub;
			*curr = tmp;
			for (auto term = curr->terms.begin(); term != curr->terms.end();) {
				if ((not term->sub.valid and not term->ltrl.valid)
					or (term->sub.valid and term->sub.terms.empty())) {
					term = curr->terms.erase(term);
				} else {
					term++;
				}
			}
		}

		for (auto term = curr->terms.begin(); term != curr->terms.end(); term++) {
			walk.push_back(&term->sub);
		}
	}

	return result;
}

parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, int net, int value, prs::attributes attr, globals g, vector<int> *next, vector<int> *covered) {
	if (not g) {
		g = globals(pr);
	}

	parse_prs::production_rule result;
	result.valid = true;
	result.keep = pr.nets[net].keep;
	result.weak = attr.weak;
	result.force = attr.force;
	result.pass = attr.pass;
	if (not attr.assume.is_tautology()) {
		result.assume = boolean::export_expression_xfactor(attr.assume, pr);
	}
	if (attr.delay_max != attributes().delay_max) {
		result.after = attr.delay_max;
	}
	result.implicant = export_guard(pr, net, value, attr, g, next, covered);
	result.action.valid = true;
	result.action.names.push_back(boolean::export_net(net, pr));
	result.action.operation = value == 1 ? "+" : "-";
	if (debug) cout << result.to_string() << endl;
	return result;
}

parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, globals g)
{
	pr.print();

	parse_prs::production_rule_set result;
	result.valid = true;

	if (pr.assume_nobackflow) {
		result.assume.push_back("nobackflow");
	}
	if (pr.assume_static) {
		result.assume.push_back("static");
	}

	if (pr.require_driven) {
		result.require.push_back("driven");
	}
	if (pr.require_stable) {
		result.require.push_back("stable");
	}
	if (pr.require_noninterfering) {
		result.require.push_back("noninterfering");
	}
	if (pr.require_adiabatic) {
		result.require.push_back("adiabatic");
	}

	if (not g) {
		g = globals(pr);
	}

	vector<int> stack, covered;
	for (int i = 0; i < (int)pr.nets.size(); i++) {
		if ((pr.drains(i, 0) > 0 or pr.drains(i, 1) > 0) and (not pr.nets[i].isNode() or (pr.sources(i, 0) == 0 and pr.sources(i, 1) == 0))) {
			stack.push_back(i);
		}
	}

	while (not stack.empty()) {
		int curr = stack.back();
		stack.pop_back();
		if (find(covered.begin(), covered.end(), curr) != covered.end()) {
			continue;
		}

		for (int i = 0; i < 2; i++) {
			if (pr.drains(curr,i) > 0) {
				auto groups = pr.attribute_groups(curr, i);
				for (auto group = groups.begin(); group != groups.end(); group++) {
					result.rules.push_back(export_production_rule(pr, curr, i, *group, g, &stack, &covered));
				}
			}
		}
	}
	return result;
}

parse_dot::graph export_bubble(const prs::bubble &bub, const prs::production_rule_set &pr)
{
	parse_dot::graph graph;
	graph.type = "digraph";
	graph.id = "bubble";

	for (int i = 0; i < (int)bub.inverted.size(); i++) {
		if (not bub.linked[i]) {
			continue;
		}
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "node";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i)));
		stmt.attributes.attributes.push_back(parse_dot::assignment_list());
		parse_dot::assignment_list &attr = stmt.attributes.attributes.back();
		attr.as.push_back(parse_dot::assignment());
		parse_dot::assignment &a = attr.as.back();
		a.first = "label";
		a.second = boolean::export_net(i, pr).to_string();
		if (bub.inverted[i]) {
			a.second = "_" + a.second;
		}
	}

	for (auto i = bub.net.begin(); i != bub.net.end(); i++)
	{
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "edge";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i->from)));
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i->to)));
		if (i->isochronic or i->bubble)
		{
			stmt.attributes.attributes.push_back(parse_dot::assignment_list());
			parse_dot::assignment_list &attr = stmt.attributes.attributes.back();

			if (i->isochronic)
			{
				attr.as.push_back(parse_dot::assignment());
				parse_dot::assignment &a = attr.as.back();
				a.first = "style";
				a.second = "dashed";
			}

			if (i->bubble)
			{
				attr.as.push_back(parse_dot::assignment());
				parse_dot::assignment &a1 = attr.as.back();
				a1.first = "arrowhead";
				a1.second = "odotnormal";
			}
		}
	}

	return graph;
}

}

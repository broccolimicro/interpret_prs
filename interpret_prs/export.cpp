#include "export.h"
#include <interpret_boolean/export.h>
#include <parse_dot/node_id.h>
#include <common/text.h>

namespace prs {

const bool debug = false;

globals::globals() {
	vdd = -1;
	gnd = -1;
}

globals::globals(ucs::variable_set &variables) {
	gnd = variables.find(ucs::variable("GND"));
	if (gnd < 0) {
		gnd = variables.define(ucs::variable("GND"));
	}
	vdd = variables.find(ucs::variable("Vdd"));
	if (vdd < 0) {
		vdd = variables.define(ucs::variable("Vdd"));
	}
}

globals::~globals() {
}

parse_prs::guard export_guard(const prs::production_rule_set &pr, ucs::variable_set &variables, int drain, int value, attributes attr, globals g, vector<int> *next, vector<int> *covered) {
	struct walker {
		int drain;
		vector<parse_prs::guard*> stack;
		attributes attr;
	};

	if (g.vdd < 0 and g.gnd < 0) {
		g = globals(variables);
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

					// Handle precharge
					if (net != drain and not stack[idx].stack.back()->terms.empty() and pr.drains(net, 1-value) != 0) {
						stack[idx].stack.back()->terms[0].pchg = export_guard(pr, variables, net, 1-value, attr, g, next, covered);
					}

					// Do the split
					int drains = pr.drains(net, value);
					if (net != drain and pr.drains(net, value, stack[idx].attr) != drains) {
						// This is a source (either a pass transistor or power)
						if (debug) cout << "exiting at node " << net << endl;
						if (stack[idx].drain != g.vdd and stack[idx].drain != g.gnd) {
							parse_prs::term arg(parse_prs::literal(export_variable_name(stack[idx].drain, variables), false, false));
							if (debug) cout << "adding term " << arg.to_string() << endl;
							stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), arg);
						}
						toErase.push_back(idx);
					} else if (drains > 1) {
						// This is a split
						if (debug) cout << drains << " drains at " << net << endl;
						stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), parse_prs::term(parse_prs::guard()));
						parse_prs::guard *sub = &stack[idx].stack.back()->terms[0].sub;
						sub->valid = true;
						sub->level = parse_prs::guard::OR;
						sub->terms.reserve(drains*2);
						for (int j = (int)pr.at(net).drainOf[value].size()-1; j >= 0; j--) {
							auto dev = pr.devs.begin()+pr.at(net).drainOf[value][j];
							if (debug) cout << pr.at(net).drainOf[value][j] << endl;
							if (dev->drain != net or dev->attr != stack[idx].attr) {
								if (debug) cout << "skipping" << endl;
								continue;
							}

							sub->terms.push_back(parse_prs::term(parse_prs::guard()));
							parse_prs::guard *subj = &sub->terms.back().sub;
							subj->valid = true;
							subj->level = parse_prs::guard::AND;

							// add this literal
							parse_prs::term arg(parse_prs::literal(export_variable_name(dev->gate, variables), dev->threshold == 0));
							if (dev->attr.size > 0.0) {
								arg.size = to_minstring(dev->attr.size);
								if (dev->attr.variant != "") {
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
							if ((int)sub->terms.size() == drains) {
								stack[idx].stack.push_back(subj);
								stack[idx].drain = dev->source;
								stack[idx].attr.set_internal();
							} else {
								stack.push_back(stack[idx]);
								stack.back().stack.push_back(subj);
								stack.back().drain = dev->source;
								stack.back().attr.set_internal();
							}
						}
					} else if (drains != 0) {
						if (debug) cout << "one drain " << net << endl;
						auto dev = pr.devs.begin()+pr.at(net).drainOf[value][0];
						for (auto di = pr.at(net).drainOf[value].begin(); di != pr.at(net).drainOf[value].end(); di++) {
							if (debug) cout << *di << endl;
							if (pr.devs[*di].drain == net and pr.devs[*di].attr == attr) {
								dev = pr.devs.begin()+*di;
								if (debug) cout << "found" << endl;
								break;
							}
						}

						// add this literal
						parse_prs::term arg(parse_prs::literal(export_variable_name(dev->gate, variables), dev->threshold == 0));
						if (dev->attr.size > 0.0) {
							arg.size = to_minstring(dev->attr.size);
							if (dev->attr.variant != "") {
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
							parse_prs::term arg(parse_prs::literal(export_variable_name(stack[idx].drain, variables), false, false));
							if (debug) cout << "adding term " << arg.to_string() << endl;
							stack[idx].stack.back()->terms.insert(stack[idx].stack.back()->terms.begin(), arg);
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
			parse_prs::term arg(parse_prs::literal(export_variable_name(i->drain, variables), false, false));
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

		for (int i = (int)curr->terms.size()-1; i >= 0; i--) {
			if ((not curr->terms[i].sub.valid and not curr->terms[i].ltrl.valid)
				or (curr->terms[i].sub.valid and curr->terms[i].sub.terms.empty())) {
				curr->terms.erase(curr->terms.begin()+i);
			}
		}

		while (curr->terms.size() == 1 and curr->terms[0].sub.valid) {
			// assigning *curr = curr->terms[0].sub; assigns curr->terms first which destroys the thing we're assigning from.
			parse_prs::guard tmp = curr->terms[0].sub;
			*curr = tmp;
			for (int i = (int)curr->terms.size()-1; i >= 0; i--) {
				if ((not curr->terms[i].sub.valid and not curr->terms[i].ltrl.valid)
					or (curr->terms[i].sub.valid and curr->terms[i].sub.terms.empty())) {
					curr->terms.erase(curr->terms.begin()+i);
				}
			}
		}

		for (int i = 0; i < (int)curr->terms.size(); i++) {
			walk.push_back(&curr->terms[i].sub);
		}
	}

	return result;
}

parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, ucs::variable_set &variables, int net, int value, prs::attributes attr, globals g, vector<int> *next, vector<int> *covered)
{
	if (g.vdd < 0 and g.gnd < 0) {
		g = globals(variables);
	}

	parse_prs::production_rule result;
	result.valid = true;
	result.keep = pr.at(net).keep;
	result.weak = attr.weak;
	result.force = attr.force;
	result.pass = attr.pass;
	if (not attr.assume.is_tautology()) {
		result.assume = export_expression_xfactor(attr.assume, variables);
	}
	if (attr.delay_max != attributes().delay_max) {
		result.after = attr.delay_max;
	}
	result.implicant = export_guard(pr, variables, net, value, attr, g, next, covered);
	result.action.valid = true;
	result.action.names.push_back(export_variable_name(net, variables));
	result.action.operation = value == 1 ? "+" : "-";
	if (debug) cout << result.to_string() << endl;
	return result;
}

parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, ucs::variable_set &variables, globals g)
{
	if (g.vdd < 0 and g.gnd < 0) {
		g = globals(variables);
	}

	parse_prs::production_rule_set result;
	result.valid = true;
	vector<int> stack, covered;
	for (int i = -(int)pr.nodes.size(); i < (int)pr.nets.size(); i++) {
		if ((pr.drains(i, 0) > 0 or pr.drains(i, 1) > 0) and (i >= 0 or (pr.sources(i, 0) == 0 and pr.sources(i, 1) == 0))) {
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
					result.rules.push_back(export_production_rule(pr, variables, curr, i, *group, g, &stack, &covered));
				}
			}
		}
	}
	return result;
}

parse_dot::graph export_bubble(const prs::bubble &bub, const ucs::variable_set &variables)
{
	parse_dot::graph graph;
	graph.type = "digraph";
	graph.id = "bubble";

	for (int i = 0; i < bub.nets; i++) {
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
		a.second = export_variable_name(i, variables).to_string();
		if (bub.inverted[i]) {
			a.second = "_" + a.second;
		}
	}

	for (int i = 0; i < bub.nodes; i++) {
		int j = -i-1;
		if (not bub.linked[bub.uid(j)]) {
			continue;
		}
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "node";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(bub.uid(j))));
		stmt.attributes.attributes.push_back(parse_dot::assignment_list());
		parse_dot::assignment_list &attr = stmt.attributes.attributes.back();
		attr.as.push_back(parse_dot::assignment());
		parse_dot::assignment &a = attr.as.back();
		a.first = "label";
		a.second = export_variable_name(j, variables).to_string();
		if (bub.inverted[bub.uid(j)]) {
			a.second = "_" + a.second;
		}
	}

	for (auto i = bub.net.begin(); i != bub.net.end(); i++)
	{
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "edge";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(bub.uid(i->from))));
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(bub.uid(i->to))));
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

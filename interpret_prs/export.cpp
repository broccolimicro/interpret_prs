/*
 * export.cpp
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include "export.h"
#include <interpret_boolean/export.h>
#include <parse_dot/node_id.h>

namespace prs {

parse_prs::guard export_guard(const prs::production_rule_set &pr, int drain, int threshold, ucs::variable_set &variables, vector<int> &next, vector<int> &covered) {
	parse_prs::guard result;
	result.valid = true;
	result.level = parse_prs::guard::AND;
	vector<pair<int, vector<parse_prs::guard*> > > stack(1, pair<int, vector<parse_prs::guard*> >(drain, vector<parse_prs::guard*>(1, &result)));
	
	// net -> elements in stack at net
	map<int, vector<vector<int> > > merge;
	bool step = true;
	while (not stack.empty() and step) {
		step = false;
		// 1. look for opportunities to merge expressions. There need to
		// be as many iterators on a device as there are items in the
		// drain's "sourceOf" list

		for (int i = 0; i < (int)stack.size(); i++) {
			auto pos = merge.insert(pair<int, vector<vector<int> > >(stack[i].first, vector<vector<int> >())).first;
			bool found = false;
			for (int j = 0; j < (int)pos->second.size() and not found; j++) {
				int idx = pos->second[j].back();
				if (stack[i].second[stack[i].second.size()-2] == stack[idx].second[stack[idx].second.size()-2]) {
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
				cout << i->first << " == " << drain << endl;
				if (i->first == drain or i->second[k].size() >= pr.at(i->first).sourceOf[threshold].size()) {
					// Do the merge
					if (i->second[k].size() > 1) {
						toErase.insert(toErase.end(), i->second[k].begin()+1, i->second[k].end());
						stack[i->second[k][0]].second.pop_back();
					}

					int idx = i->second[k][0];
					int net = i->first;
					covered.push_back(net);

					// Handle precharge
					if (net != drain and not stack[idx].second.back()->terms.empty() and not pr.at(net).drainOf[1-threshold].empty()) {
						stack[idx].second.back()->terms[0].pchg = export_guard(pr, net, 1-threshold, variables, next, covered);
					}

					// Do the split
					if (pr.at(net).drainOf[threshold].size() > 1) {
						stack[idx].second.back()->terms.insert(stack[idx].second.back()->terms.begin(), parse_prs::term(parse_prs::guard()));
						parse_prs::guard *sub = &stack[idx].second.back()->terms[0].sub;
						sub->valid = true;
						sub->level = parse_prs::guard::OR;
						sub->terms.reserve(pr.at(net).drainOf[threshold].size()*2);
						for (int j = (int)pr.at(net).drainOf[threshold].size()-1; j >= 0; j--) {
							int dev = pr.at(net).drainOf[threshold][j];

							sub->terms.push_back(parse_prs::term(parse_prs::guard()));
							parse_prs::guard *subj = &sub->terms.back().sub;
							subj->valid = true;
							subj->level = parse_prs::guard::AND;

							// add this literal
							parse_prs::term arg(parse_prs::literal(export_variable_name(pr.devs[dev].gate, variables), pr.devs[dev].threshold == 0));

							subj->terms.insert(subj->terms.begin(), arg);

							if (j != 0) {
								stack.push_back(stack[idx]);
								stack.back().second.push_back(subj);
								stack.back().first = pr.devs[dev].source;
							} else {
								stack[idx].second.push_back(subj);
								stack[idx].first = pr.devs[dev].source;
							}
						}
					} else if (not pr.at(net).drainOf[threshold].empty()) {
						int dev = pr.at(net).drainOf[threshold].back();

						// add this literal
						parse_prs::term arg(parse_prs::literal(export_variable_name(pr.devs[dev].gate, variables), pr.devs[dev].threshold == 0));

						stack[idx].second.back()->terms.insert(stack[idx].second.back()->terms.begin(), arg);

						stack[idx].first = pr.devs[dev].source;
					} else {
						parse_prs::term arg(parse_prs::literal(export_variable_name(stack[idx].first, variables), false, false));
						stack[idx].second.back()->terms.insert(stack[idx].second.back()->terms.begin(), arg);
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
			stack[toErase[i]].second.clear();
			stack.erase(stack.begin() + toErase[i]);
		}
	}

	//   a. if none of the items in the stack pass the test, then we
	//   stop here and those nodes become virtual nodes.
	for (auto i = stack.begin(); i != stack.end(); i++) {
		parse_prs::term arg(parse_prs::literal(export_variable_name(i->first, variables), false, false));
		i->second.back()->terms.insert(i->second.back()->terms.begin(), arg);

		if (find(covered.begin(), covered.end(), i->first) == covered.end()) {
			next.push_back(i->first);
		}
	}
	sort(next.begin(), next.end());
	next.erase(unique(next.begin(), next.end()), next.end());

	// Clean up final result
	/*stack.clear();
	stack.push_back(pair<int, parse_prs::guard*>(-1, &result));
	while (not stack.empty()) {
		auto curr = stack.back().second;
		stack.pop_back();

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
			stack.push_back(pair<int, parse_prs::guard*>(-1, &curr->terms[i].sub));
		}
	}*/

	return result;
}

parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, int net, int value, ucs::variable_set &variables, vector<int> &next, vector<int> &covered)
{
	parse_prs::production_rule result;
	result.valid = true;
	/*if (not pr.assume.is_tautology()) {
		result.assume = export_expression_xfactor(pr.assume, variables);
	}*/
	result.implicant = export_guard(pr, net, 1-value, variables, next, covered);
	result.action.valid = true;
	result.action.names.push_back(export_variable_name(net, variables));
	result.action.operation = value == 1 ? "+" : "-";
	return result;
}

parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, ucs::variable_set &variables)
{
	cout << "nets " << pr.nets.size() << endl;
	for (int i = 0; i < (int)pr.nets.size(); i++) {
		cout << "net " << i << ": gateOf=" << to_string(pr.nets[i].gateOf) << " sourceOf=" << to_string(pr.nets[i].sourceOf[0]) << to_string(pr.nets[i].sourceOf[1]) << " drainOf=" << to_string(pr.nets[i].drainOf[0]) << to_string(pr.nets[i].drainOf[1])  << endl;
	}

	cout << "nodes " << pr.nodes.size() << endl;
	for (int i = 0; i < (int)pr.nodes.size(); i++) {
		cout << "node " << i << ": gateOf=" << to_string(pr.nodes[i].gateOf) << " sourceOf=" << to_string(pr.nodes[i].sourceOf[0]) << to_string(pr.nodes[i].sourceOf[1]) << " drainOf=" << to_string(pr.nodes[i].drainOf[0]) << to_string(pr.nodes[i].drainOf[1])  << endl;
	}

	cout << "devs " << pr.devs.size() << endl;
	for (int i = 0; i < (int)pr.devs.size(); i++) {
		cout << "dev " << i << ": source=" << pr.devs[i].source << " gate=" << pr.devs[i].gate << " drain=" << pr.devs[i].drain << " threshold=" << pr.devs[i].threshold << endl;
	}

	parse_prs::production_rule_set result;
	result.valid = true;
	vector<int> stack, covered;
	stack.reserve(pr.nets.size()*10);
	for (int i = 0; i < (int)pr.nets.size(); i++) {
		stack.push_back(i);
	}

	while (not stack.empty()) {
		cout << "export stack=" << to_string(stack) << endl;
		int curr = stack.back();
		stack.pop_back();
		cout << "curr=" << curr << endl;

		for (int i = 0; i < 2; i++) {
			if (not pr.at(curr).drainOf[1-i].empty()) {
				result.rules.push_back(export_production_rule(pr, curr, i, variables, stack, covered));
			}
		}
	}
	return result;
}

/*parse_dot::graph export_bubble(const prs::bubble &bub, const ucs::variable_set &variables)
{
	parse_dot::graph graph;
	graph.type = "digraph";
	graph.id = "bubble";

	for (size_t i = 0; i < variables.nodes.size(); i++)
	{
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "node";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i)));
		stmt.attributes.attributes.push_back(parse_dot::assignment_list());
		parse_dot::assignment_list &attr = stmt.attributes.attributes.back();
		attr.as.push_back(parse_dot::assignment());
		parse_dot::assignment &a = attr.as.back();
		a.first = "label";
		a.second = variables.nodes[i].to_string();
		if (bub.inverted[i]) {
			a.second = "_" + a.second;
		}
	}

	for (auto i = bub.net.begin(); i != bub.net.end(); i++)
	{
		graph.statements.push_back(parse_dot::statement());
		parse_dot::statement &stmt = graph.statements.back();
		stmt.statement_type = "edge";
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i->first.first)));
		stmt.nodes.push_back(new parse_dot::node_id("V" + to_string(i->first.second)));
		if (i->second.first || i->second.second)
		{
			stmt.attributes.attributes.push_back(parse_dot::assignment_list());
			parse_dot::assignment_list &attr = stmt.attributes.attributes.back();

			if (i->second.first)
			{
				attr.as.push_back(parse_dot::assignment());
				parse_dot::assignment &a = attr.as.back();
				a.first = "style";
				a.second = "dashed";
			}

			if (i->second.second)
			{
				attr.as.push_back(parse_dot::assignment());
				parse_dot::assignment &a1 = attr.as.back();
				a1.first = "arrowhead";
				a1.second = "odotnormal";
			}
		}
	}

	return graph;
}*/

}

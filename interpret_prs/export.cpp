/*
 * export.cpp
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include "export.h"
#include <interpret_boolean/export.h>
#include <parse_dot/node_id.h>

parse_prs::production_rule export_production_rule(const prs::production_rule &pr, ucs::variable_set &variables)
{
	parse_prs::production_rule result;
	result.valid = true;
	result.implicant = export_expression_xfactor(pr.guard, variables);
	result.action = export_composition(pr.local_action, variables);
	return result;
}

parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &prs, ucs::variable_set &variables)
{
	parse_prs::production_rule_set result;
	result.valid = true;
	for (int i = 0; i < (int)prs.rules.size(); i++)
		result.rules.push_back(export_production_rule(prs.rules[i], variables));
	return result;
}

parse_dot::graph export_bubble(const prs::bubble &bub, const ucs::variable_set &variables)
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
}

void export_devices(parse_spice::subckt &ckt, const prs::production_rule &pr, ucs::variable_set &variables) {

}

parse_spice::subckt export_subckt(const prs::production_rule_set &prs, ucs::variable_set &variables) {

}

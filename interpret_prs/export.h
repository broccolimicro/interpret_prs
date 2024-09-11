/*
 * export.h
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#pragma once

#include <common/standard.h>

#include <ucs/variable.h>
#include <prs/production_rule.h>
#include <prs/bubble.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>
#include <parse_dot/graph.h>
#include <parse_spice/subckt.h>

namespace prs {

struct globals {
	globals();
	globals(ucs::variable_set &variables);
	~globals();

	int vdd;
	int gnd;
};

parse_prs::guard export_guard(const prs::production_rule_set &pr, ucs::variable_set &variables, int drain, int value, bool weak=false, bool pass=false, boolean::cover assume=1, globals g=globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, ucs::variable_set &variables, int net, int value, bool weak=false, bool pass=false, boolean::cover assume=1, globals g=globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, ucs::variable_set &variables, globals g=globals());

parse_dot::graph export_bubble(const prs::bubble &bub, const ucs::variable_set &variables);

}

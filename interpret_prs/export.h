#pragma once

#include <prs/production_rule.h>
#include <prs/bubble.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>
#include <parse_dot/graph.h>
#include <parse_spice/subckt.h>

#include <interpret_boolean/interface.h>

namespace prs {

struct globals {
	globals();
	globals(const prs::production_rule_set &pr);
	~globals();

	int vdd;
	int gnd;

	operator bool();
};

parse_prs::guard export_guard(const prs::production_rule_set &pr, int drain, int value, prs::attributes attr=attributes(), globals g=globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, int net, int value, prs::attributes attr=attributes(), globals g=globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, globals g=globals());

parse_dot::graph export_bubble(const prs::bubble &bub);

}

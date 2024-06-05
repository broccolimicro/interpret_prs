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

parse_prs::production_rule export_production_rule(const prs::production_rule &pr, ucs::variable_set &variables);
parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &prs, ucs::variable_set &variables);

parse_dot::graph export_bubble(const prs::bubble &bub, const ucs::variable_set &variables);

void export_devices(parse_spice::subckt &ckt, const prs::production_rule &pr, ucs::variable_set &variables);
parse_spice::subckt export_subckt(const prs::production_rule_set &prs, ucs::variable_set &variables);

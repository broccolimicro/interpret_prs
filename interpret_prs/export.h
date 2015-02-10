/*
 * export.h
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include <common/standard.h>

#include <boolean/variable.h>
#include <prs/production_rule.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>

#ifndef interpret_prs_export_h
#define interpret_prs_export_h

parse_prs::production_rule export_production_rule(const prs::production_rule &pr, boolean::variable_set &variables);
parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &prs, boolean::variable_set &variables);

#endif

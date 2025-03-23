#pragma once

#include <parse/tokenizer.h>

#include <prs/production_rule.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>

namespace prs {

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int driver, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define);
void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define);
void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, attributes attr, int default_id, tokenizer *tokens, bool auto_define);

}

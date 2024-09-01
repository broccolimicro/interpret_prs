#pragma once

#include <common/standard.h>

#include <parse/tokenizer.h>

#include <ucs/variable.h>
#include <prs/production_rule.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>

namespace prs {

void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define);
void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define);

}

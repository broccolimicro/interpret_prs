/*
 * import.cpp
 *
 *  Created on: Feb 6, 2015
 *      Author: nbingham
 */

#include "import.h"
#include <interpret_boolean/import.h>

prs::production_rule import_production_rule(const parse_prs::production_rule &syntax, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	prs::production_rule result;
	result.guard = import_cover(syntax.implicant, variables, default_id, tokens, auto_define);
	result.local_action = import_cover(syntax.action, variables, default_id, tokens, auto_define);
	return result;
}

prs::production_rule_set import_production_rule_set(const parse_prs::production_rule_set &syntax, ucs::variable_set &variables, int default_id, tokenizer *tokens, bool auto_define)
{
	prs::production_rule_set result;

	if (syntax.region != "")
		default_id = atoi(syntax.region.c_str());

	for (int i = 0; i < (int)syntax.rules.size(); i++)
		result.rules.push_back(import_production_rule(syntax.rules[i], variables, default_id, tokens, auto_define));

	for (int i = 0; i < (int)syntax.regions.size(); i++)
	{
		prs::production_rule_set temp = import_production_rule_set(syntax.regions[i], variables, default_id, tokens, auto_define);
		result.rules.insert(result.rules.end(), temp.rules.begin(), temp.rules.end());
	}

	return result;
}

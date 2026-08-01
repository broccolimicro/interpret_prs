#pragma once

#include <parse/tokenizer.h>

#include <prs/production_rule.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>
#include <parse_expression/import.h>

namespace parse_prs {

struct BooleanExpressionImporter : parse_expression::LValuedImporter<boolean::cover, std::string> {
	ucs::Netlist symbols;
	vector<int> region;
	bool autoDefine;

	BooleanExpressionImporter(ucs::Netlist symbols, int region = 0, bool autoDefine = false);
	~BooleanExpressionImporter();

	boolean::cover L_to_T(std::string lval, tokenizer *tokens) const override;
	std::string T_to_L(boolean::cover expr, tokenizer *tokens) const override;

	bool is_lvalue(const parse_expression::expression &syntax) const override;
	
	std::string import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const override;

	void push_properties(parse_expression::operation op, const vector<parse_expression::expression::argument> &args, tokenizer *tokens) override;
	void pop_properties(parse_expression::operation op) override;

	std::string import_modifier(parse_expression::operation op, vector<std::string> args, tokenizer *tokens) const override;

	boolean::cover import_unary(parse_expression::operation op, boolean::cover expr, tokenizer *tokens) const override;
	boolean::cover import_binary(parse_expression::operation op, boolean::cover left, boolean::cover right, tokenizer *tokens) const override;
	boolean::cover import_modifier(parse_expression::operation op, vector<boolean::cover> args, tokenizer *tokens) const override;
};

boolean::cover import_cover(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region = 0, bool auto_define = false);
boolean::cube import_cube(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region = 0, bool auto_define = false);

struct BooleanCompositionImporter : parse_expression::Importer<boolean::cover> {
	ucs::Netlist symbols;
	vector<int> region;
	bool autoDefine;

	BooleanCompositionImporter(ucs::Netlist symbols, int region = 0, bool autoDefine = false);
	~BooleanCompositionImporter();

	boolean::cube import_assignment(const assignment &syntax, tokenizer *tokens) const;
	boolean::cover import_term(const parse_expression::expression::argument &syntax, tokenizer *tokens) const override;
	void push_properties(parse_expression::operation op, const vector<parse_expression::expression::argument> &args, tokenizer *tokens) override;
	void pop_properties(parse_expression::operation op) override;
	boolean::cover import_binary(parse_expression::operation op, boolean::cover left, boolean::cover right, tokenizer *tokens) const override;
	boolean::cover import_modifier(parse_expression::operation op, vector<boolean::cover> args, tokenizer *tokens) const override;
};

boolean::cube import_assignment(const assignment &syntax, ucs::Netlist nets, tokenizer *tokens, int region = 0, bool auto_define = false);
boolean::cover import_choice(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region = 0, bool auto_define = false);
boolean::cube import_parallel(const parse_expression::expression &syntax, ucs::Netlist nets, tokenizer *tokens, int region = 0, bool auto_define = false);

}

namespace prs {

vector<int> import_guard(const parse_prs::guard &syntax, prs::production_rule_set &pr, int drain, int driver, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define);
void import_production_rule(const parse_prs::production_rule &syntax, prs::production_rule_set &pr, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define);
void import_production_rule_set(const parse_prs::production_rule_set &syntax, prs::production_rule_set &pr, int vdd, int gnd, prs::attributes attr, int default_id, tokenizer *tokens, bool auto_define);

}

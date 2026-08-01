#pragma once

#include <prs/production_rule.h>
#include <prs/bubble.h>

#include <parse_prs/production_rule.h>
#include <parse_prs/production_rule_set.h>
#include <parse_dot/graph.h>
#include <parse_spice/subckt.h>
#include <interpret_boolean/export.h>

namespace prs {

struct globals {
	globals();
	globals(const prs::production_rule_set &pr);
	~globals();

	int vdd;
	int gnd;

	operator bool();
};

}

namespace parse_prs {

struct BooleanExpressionExporter : boolean::ExpressionExporter {
	ucs::ConstNetlist nets;

	BooleanExpressionExporter(ucs::ConstNetlist nets);
	~BooleanExpressionExporter();

	parse_expression::operation export_operator(int func) const override;
	const parse_expression::precedence_set &precedence() const override;

	parse_expression::expression::argument export_constant(int value) const override;
	parse_expression::expression::argument export_literal(size_t index) const override;
};

parse_expression::expression export_expression(boolean::cube expr, ucs::ConstNetlist nets);
parse_expression::expression export_expression(boolean::cover expr, ucs::ConstNetlist nets);
parse_expression::expression export_expression_xfactor(boolean::cover expr, ucs::ConstNetlist nets);
parse_expression::expression export_expression_hfactor(boolean::cover expr, ucs::ConstNetlist nets);

struct BooleanCompositionExporter : boolean::ExpressionExporter {
	ucs::ConstNetlist nets;

	BooleanCompositionExporter(ucs::ConstNetlist nets);
	~BooleanCompositionExporter();

	parse_expression::operation export_operator(int func) const override;
	const parse_expression::precedence_set &precedence() const override;

	parse_expression::expression::argument export_constant(int value) const override;
	parse_expression::expression::argument export_literal(size_t index) const override;
	assignment export_assignment(size_t index, int value) const;
	parse_expression::expression::argument export_term(size_t index, int value) const override;
};

assignment export_assignment(size_t index, int value, ucs::ConstNetlist nets);
parse_expression::expression export_composition(boolean::cube expr, ucs::ConstNetlist nets);
parse_expression::expression export_composition(boolean::cover expr, ucs::ConstNetlist nets);
parse_expression::expression export_composition_xfactor(boolean::cover expr, ucs::ConstNetlist nets);
parse_expression::expression export_composition_hfactor(boolean::cover expr, ucs::ConstNetlist nets);


parse_prs::guard export_guard(const prs::production_rule_set &pr, int drain, int value, prs::attributes attr=prs::attributes(), prs::globals g=prs::globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule export_production_rule(const prs::production_rule_set &pr, int net, int value, prs::attributes attr=prs::attributes(), prs::globals g=prs::globals(), vector<int> *next=nullptr, vector<int> *covered=nullptr);
parse_prs::production_rule_set export_production_rule_set(const prs::production_rule_set &pr, prs::globals g=prs::globals());

parse_dot::graph export_bubble(const prs::bubble &bub, const prs::production_rule_set &pr);

}

// Copyright 2024 Kyrylo Shyshko
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//    http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "symcalc/symcalc.hpp"

//
// symcalc.cpp:
// Defines all 'inside' classes of SymCalc, like EquationBase, Variable, Mult, etc.
// Defines constants
//


namespace symcalc{
	
bool SYMCALC_AUTO_SIMPLIFY = true;
	
EquationBase* to_equation(SYMCALC_VALUE_TYPE num){
	return new EquationValue(num);
}



EquationBase::EquationBase(std::string el_type){
	this->type = el_type;
}

EquationBase::EquationBase(const EquationBase& lvalue){
	type = lvalue.type;
}

EquationBase::~EquationBase(){
	// std::cout << "destructor" << std::endl;
}


EquationBase* EquationBase::_simplify() const {return copy(this);};


Variable::Variable(SYMCALC_VAR_NAME_TYPE name) : EquationBase("var"), name(name) {}

Variable::Variable(const Variable& lvalue) : EquationBase(lvalue){
	name = lvalue.name;
}

Variable::~Variable(){
	
}


std::vector<SYMCALC_VAR_NAME_TYPE> Variable::list_variables() const{
	return std::vector<SYMCALC_VAR_NAME_TYPE>({this->name});
}

std::string Variable::txt() const{
	return name;
}

SYMCALC_VALUE_TYPE Variable::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return var_hash[this->name];
}



EquationBase* Variable::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	if(var != this->name){
		return new EquationValue(0.0);
	}
	return new EquationValue(1.0);
}


EquationBase* Variable::_simplify() const{
	return copy(this);
}

EquationBase* Variable::_copy_equation_base() const{
	const Variable* casted = dynamic_cast<const Variable*>(this);
	return new Variable(*casted);
}
void Variable::_delete_equation_base(){
	Variable* casted = dynamic_cast<Variable*>(this);
	delete casted;
}


EquationValue::EquationValue(SYMCALC_VALUE_TYPE value) : EquationBase("val"), value(value) {
	this->ready_txt = std::to_string(value);
	this->ready_txt.erase(this->ready_txt.find_last_not_of('0') + 1, std::string::npos);
	this->ready_txt.erase(this->ready_txt.find_last_not_of('.') + 1, std::string::npos);
}

EquationValue::EquationValue(const EquationValue& lvalue) : EquationBase(lvalue), value(lvalue.value), ready_txt(lvalue.ready_txt){
}

EquationValue::~EquationValue(){
	
}

std::vector<SYMCALC_VAR_NAME_TYPE> EquationValue::list_variables() const{
	return std::vector<SYMCALC_VAR_NAME_TYPE>();
}

std::string EquationValue::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE EquationValue::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return value;
}

EquationBase* EquationValue::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	return (new EquationValue(0));
}


EquationBase* EquationValue::_simplify() const{
	return copy(this);
}


EquationBase* EquationValue::_copy_equation_base() const{
	const EquationValue* casted = dynamic_cast<const EquationValue*>(this);
	return new EquationValue(*casted);
}
void EquationValue::_delete_equation_base(){
	EquationValue* casted = dynamic_cast<EquationValue*>(this);
	delete casted;
}




Constant::Constant(SYMCALC_VAR_NAME_TYPE name, SYMCALC_VALUE_TYPE value) : EquationValue(value), name(name) {
	this->type = "const";
}

Constant::Constant(const Constant& lvalue) : EquationValue(lvalue){
	this->name = lvalue.name;
}

std::string Constant::txt() const{
	return this->name;
}

EquationBase* Constant::_copy_equation_base() const{
	const Constant* casted = dynamic_cast<const Constant*>(this);
	return new Constant(*casted);
}

void Constant::_delete_equation_base(){
	Constant* casted = dynamic_cast<Constant*>(this);
	delete casted;
}





Sum::Sum(std::vector<EquationBase*> inp_elements) : EquationBase("sum"){
	std::vector<EquationBase*> extracted_elements;
	
	for(EquationBase* &el : inp_elements){	
		
		if(el->type == "sum"){ // if the element is a sum - extract its elements into the current sum object
			Sum* sum_element = dynamic_cast<Sum*>(el); // dynamic cast of EquationBase* to Sum* to get the .elements attribute
			for(EquationBase* sum_el_part : sum_element->elements){
				extracted_elements.push_back(sum_el_part);
			};
		}else{
			extracted_elements.push_back(el);
		}
	}
	
	this->elements = extracted_elements;
	
	ready_txt = "(" + this->elements[0]->txt() + ")";
	
	for(size_t i = 1; i < this->elements.size(); i++){
		ready_txt += " + (" + this->elements[i]->txt() + ")";
	}
}





Sum::Sum(const Sum& lvalue) : EquationBase(lvalue), elements(), ready_txt(lvalue.ready_txt){
	for(const EquationBase* lvalue_el : lvalue.elements){
		elements.push_back(copy(lvalue_el));
	}
	
}

Sum::~Sum(){
	for(EquationBase* el : elements){
		delete_equation_base(el);
	}
}



std::vector<SYMCALC_VAR_NAME_TYPE> Sum::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> vars;
	
	std::vector<SYMCALC_VAR_NAME_TYPE> temp_vars;
	
	for(EquationBase* el : elements){
		temp_vars = el->list_variables();
		
		for(SYMCALC_VAR_NAME_TYPE var : temp_vars){
			if(!include(vars, var)){
				vars.push_back(var);
			}
		}
		
	}
	
	return vars;
}


std::string Sum::txt() const{
	return ready_txt;
}

SYMCALC_VALUE_TYPE Sum::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	SYMCALC_VALUE_TYPE result {0};
	
	for(EquationBase* el : this->elements){
		result += el->eval(var_hash);
	}
	
	return result;
}

EquationBase* Sum::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	
	std::vector<EquationBase*> derivs;
	derivs.reserve(elements.size());
	
	for(EquationBase* el : elements){
		derivs.push_back(el->_derivative(var));
	}
	
	return new Sum(derivs);
}


EquationBase* Sum::_simplify() const{
	std::vector<EquationBase*> els;

	
	for(EquationBase* element : elements){
		EquationBase* simplified = element->_simplify();
		if(simplified->type == "val"){
			EquationValue* casted = dynamic_cast<EquationValue*>(simplified);
			if(casted->value != 0){
				els.push_back(simplified);
			}
		}else{
			els.push_back(simplified);
		}
	}
	
	if(els.size() == 1){
		return els[0];
	}
	
	if(els.size() == 0){
		return new EquationValue(0);
	}
	
	return new Sum(els);
}

EquationBase* Sum::_copy_equation_base() const{
	const Sum* casted = dynamic_cast<const Sum*>(this);
	return new Sum(*casted);
}
void Sum::_delete_equation_base(){
	Sum* casted = dynamic_cast<Sum*>(this);
	delete casted;
}



Negate::Negate(EquationBase* eq) : EquationBase("neg"), eq(eq) {
	this->ready_txt = "-(" + eq->txt() + ")";
}

Negate::Negate(const Negate& lvalue) : EquationBase(lvalue), eq(nullptr), ready_txt(lvalue.ready_txt){
	const EquationBase* lvalue_eq = lvalue.eq;
	eq = copy(lvalue_eq);
}

Negate::~Negate(){
	delete_equation_base(eq);
}

std::vector<SYMCALC_VAR_NAME_TYPE> Negate::list_variables() const{
	return eq->list_variables();
}


std::string Negate::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE Negate::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return -eq->eval(var_hash);
}

EquationBase* Negate::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	return new Negate(eq->_derivative(var));
}


EquationBase* Negate::_simplify() const{
	EquationBase* simplified = eq->_simplify();
	if(eq->type == "neg"){
		Negate* casted = dynamic_cast<Negate*>(simplified);
		return casted->eq;
	}else{
		return new Negate(simplified);
	}
}

EquationBase* Negate::_copy_equation_base() const{
	const Negate* casted = dynamic_cast<const Negate*>(this);
	return new Negate(*casted);
}
void Negate::_delete_equation_base(){
	Negate* casted = dynamic_cast<Negate*>(this);
	delete casted;
}



Mult::Mult(std::vector<EquationBase*> inp_elements) : EquationBase("mult"){
	std::vector<EquationBase*> extracted_elements;
	
	for(EquationBase* &el : inp_elements){	
		
		if(el->type == "mult"){ // if the element is a sum - extract its elements into the current sum object
			Mult* mult_element = dynamic_cast<Mult*>(el); // dynamic cast of EquationBase* to Sum* to get the .elements attribute
			for(EquationBase* mult_el_part : mult_element->elements){
				extracted_elements.push_back(mult_el_part);
			};
		}else{
			extracted_elements.push_back(el);
		}
	}
	
	this->elements = extracted_elements;
	
	ready_txt = "(" + this->elements[0]->txt() + ")";
	
	for(size_t i = 1; i < this->elements.size(); i++){
		ready_txt += " * (" + this->elements[i]->txt() + ")";
	}
}

Mult::Mult(const Mult& lvalue) : EquationBase(lvalue), elements(), ready_txt(lvalue.ready_txt){
	for(const EquationBase* lvalue_el : lvalue.elements){
		elements.push_back(copy(lvalue_el));
	}
}

Mult::~Mult(){
	for(EquationBase* el : elements){
		delete_equation_base(el);
	}
}

std::vector<SYMCALC_VAR_NAME_TYPE> Mult::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> vars;
	std::vector<SYMCALC_VAR_NAME_TYPE> temp_vars;
	
	
	for(const EquationBase* el : elements){
		temp_vars = el->list_variables();
		for(SYMCALC_VAR_NAME_TYPE var : temp_vars){
			if(!include(vars, var)){
				vars.push_back(var);
			}
		}
	}
	
	return vars;
}


std::string Mult::txt() const{
	return ready_txt;
}

SYMCALC_VALUE_TYPE Mult::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	SYMCALC_VALUE_TYPE result (1.0);
	
	for(EquationBase* el : this->elements){
		result *= el->eval(var_hash);
	}
	
	return result;
}

EquationBase* Mult::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	
	std::vector<EquationBase*> mults_to_sum;
	mults_to_sum.reserve(elements.size());
	
	
	std::vector<EquationBase*> els_to_mult;
	els_to_mult.reserve(elements.size());
	
	for(size_t el_i = 0; el_i < elements.size(); el_i++){
		
		els_to_mult.push_back(elements[el_i]->_derivative(var));
		
		for(size_t el_noderiv_i = 0; el_noderiv_i < elements.size(); el_noderiv_i++){
			if(el_noderiv_i == el_i){
				continue;
			}
			els_to_mult.push_back(copy(elements[el_noderiv_i]));
		}
		
		mults_to_sum.push_back(new Mult(els_to_mult));
		els_to_mult.clear();
	}
	
	return new Sum(mults_to_sum);
}



EquationBase* Mult::_simplify() const{
	
	std::vector<EquationBase*> els;
	
	if(elements.size() == 1){
		return elements[0]->_simplify();
	}
	
	SYMCALC_VALUE_TYPE coeff = 1.0; // Multiply all numerical values to a single coefficient

	for(EquationBase* el : elements){
		EquationBase* simplified = el->_simplify();
		if(simplified->type == "val"){
			EquationValue* casted = dynamic_cast<EquationValue*>(simplified);
			if(casted->value == 0){ // If zero, stop loop and output zero, since anything * 0 is 0
				for(EquationBase* el : els){
					delete_equation_base(el);
				}
				return new EquationValue(0);
			}else{
				coeff *= casted->value;				
			}
		}else{
			els.push_back(simplified);
		}
	}
	
	if(coeff != 1 || els.size() == 0){
		els.insert(els.begin(), new EquationValue(coeff));
	}
	
	if(els.size() == 1){
		return els[0];
	}	

	return new Mult(els);
}


EquationBase* Mult::_copy_equation_base() const{
	const Mult* casted = dynamic_cast<const Mult*>(this);
	return new Mult(*casted);
}
void Mult::_delete_equation_base(){
	Mult* casted = dynamic_cast<Mult*>(this);
	delete casted;
}





Div::Div(EquationBase* dividend, EquationBase* divisor) : EquationBase("div"), dividend(dividend), divisor(divisor){
	this->ready_txt = "(" + dividend->txt() + ") / (" + divisor->txt() + ")"; 
}

Div::Div(const Div& lvalue) : EquationBase(lvalue), ready_txt(lvalue.ready_txt){
	const EquationBase* lvalue_dividend = lvalue.dividend;
	dividend = copy(lvalue_dividend);
	
	const EquationBase* lvalue_divisor = lvalue.divisor;
	divisor = copy(lvalue_divisor);
}

Div::~Div(){
	delete_equation_base(dividend);
	delete_equation_base(divisor);
}

std::vector<SYMCALC_VAR_NAME_TYPE> Div::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> vars = dividend->list_variables();
	std::vector<SYMCALC_VAR_NAME_TYPE> temp_vars = divisor->list_variables();
	for(std::string el : temp_vars){
		if(!include(vars, el)){
			vars.push_back(el);
		}
	}
	return vars;
}

std::string Div::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE Div::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return dividend->eval(var_hash) / divisor->eval(var_hash);
}

EquationBase* Div::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	
	// Derivative of f(x) / g(x)
	// = (f'(x) * g(x) - f(x) * g'(x)) / g(x)^2
	
	EquationBase* left_part = new Mult({dividend->_derivative(var), copy(divisor)}); // f'(x) * g(x)
	EquationBase* right_part = new Mult({copy(dividend), divisor->_derivative(var)}); // f(x) * g'(x)
	
	EquationBase* top = new Sum({left_part, new Negate(right_part)}); // f'(x) * g(x) - f(x) * g'(x)
	
	EquationBase* bottom = new Power(copy(divisor), to_equation(2)); // g(x) ^ 2
	
	return new Div(top, bottom); // (f'(x) * g(x) - f(x) * g'(x)) / g(x)^2
}



EquationBase* Div::_simplify() const{
	
	EquationBase* dividend_s = dividend->_simplify();
	EquationBase* divisor_s = divisor->_simplify();
	
	return new Div(dividend_s, divisor_s);
}


EquationBase* Div::_copy_equation_base() const{
	const Div* casted = dynamic_cast<const Div*>(this);
	return new Div(*casted);
}
void Div::_delete_equation_base(){
	Div* casted = dynamic_cast<Div*>(this);
	delete casted;
}





Power::Power(EquationBase* base, EquationBase* power) : EquationBase("pow"), base(base), power(power){
	this->ready_txt = "(" + base->txt() + ") ^ (" + power->txt() + ")";
}

Power::Power(const Power& lvalue) : EquationBase(lvalue), ready_txt(lvalue.ready_txt){
	const EquationBase* lvalue_base = lvalue.base;
	this->base = copy(lvalue_base);
	
	const EquationBase* lvalue_power = lvalue.power;
	this->power = copy(lvalue_power);
}

Power::~Power(){
	delete_equation_base(base);
	delete_equation_base(power);
}

std::vector<SYMCALC_VAR_NAME_TYPE> Power::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> vars = base->list_variables();
	std::vector<SYMCALC_VAR_NAME_TYPE> temp_vars = power->list_variables();
	for(std::string el : temp_vars){
		if(!include(vars, el)){
			vars.push_back(el);
		}
	}
	return vars;
}



std::string Power::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE Power::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::pow(base->eval(var_hash), power->eval(var_hash));
}

EquationBase* Power::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	if(power->type == "val" || power->type == "const"){
	
		// If the power is a number or a constant then return the following:
		//
		// f = g ^ C
		// 
		// df/dx = C * g^(C - 1) * dg/dx
	
		const EquationValue* power_eq = dynamic_cast<const EquationValue*>(power);
		EquationBase* power_copy = copy(power); // C
		EquationBase* base_copy = copy(base); // g
		EquationBase* new_eq_value = new EquationValue(power_eq->value - 1); // C - 1
		EquationBase* base_deriv = base->_derivative(var); // dg/dx
		EquationBase* power_to_mult = new Power(base_copy, new_eq_value); // g ^ C - 1
		
		EquationBase* final_mult = new Mult({power_copy, power_to_mult, base_deriv}); // C * g^(C - 1) * dg/dx
		return final_mult;
	}else{
		// derivative of f(x) ^ g(x) = f(x)^g(x) * (g'(x) * ln(f(x)) + f'(x) * g(x) / f(x))
		EquationBase* mult_l_part = new Power(copy(base), copy(power));
		
		EquationBase* sum_l_part = new Mult({power->_derivative(var), new Ln(copy(base))});
		
		EquationBase* sum_r_part_mult = new Mult({base->_derivative(var), copy(power)});
		EquationBase* sum_r_part = new Div(sum_r_part_mult, copy(base));
		
		EquationBase* mult_r_part = new Sum({sum_l_part, sum_r_part});
		
		return new Mult({mult_l_part, mult_r_part});
	}
}



EquationBase* Power::_simplify() const{

	EquationBase* base_s = base->_simplify();
	EquationBase* power_s = power->_simplify();

	if(base_s->type == "val"){
		EquationValue* casted = dynamic_cast<EquationValue*>(base_s);
		if(casted->value == 0){
			return new EquationValue(0);
		}else if(casted->value == 1){
			return new EquationValue(1);
		}
	}
	
	if(power_s->type=="val"){
		EquationValue* casted = dynamic_cast<EquationValue*>(power_s);
		if(casted->value == 0){
			return new EquationValue(1);
		}else if(casted->value == 1){
			return base_s;
		}
	}
	
	return new Power(base_s, power_s);
	
}

EquationBase* Power::_copy_equation_base() const{
	const Power* casted = dynamic_cast<const Power*>(this);
	return new Power(*casted);
}
void Power::_delete_equation_base(){
	Power* casted = dynamic_cast<Power*>(this);
	delete casted;
}




Log::Log(EquationBase* eq, EquationBase* base) : EquationBase("log"), eq(eq), base(base){
	this->ready_txt = "log_(" + base->txt() + ")(" + eq->txt() + ")";
}

Log::Log(const Log& lvalue) : EquationBase(lvalue), ready_txt(lvalue.ready_txt){
	const EquationBase* lvalue_eq = lvalue.eq;
	const EquationBase* lvalue_base = lvalue.base;
	this->eq = copy(lvalue_eq);
	this->base = copy(lvalue_base);
}

Log::~Log(){
	delete_equation_base(eq);
	delete_equation_base(base);
}

std::vector<SYMCALC_VAR_NAME_TYPE> Log::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> variables = eq->list_variables();
	for(SYMCALC_VAR_NAME_TYPE var : base->list_variables()){
		if(!include(variables, var)){
			variables.push_back(var);
		}
	}
	return variables;
}


std::string Log::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE Log::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::log(eq->eval(var_hash)) / std::log(base->eval(var_hash));
}

EquationBase* Log::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	EquationBase* div = new Div(eq->_derivative(var), copy(eq));
	EquationBase* natural_log = new Ln(copy(this->base));
	return new Mult({div, natural_log});
}


EquationBase* Log::_simplify() const{
	EquationBase* simplified_eq = eq->_simplify();
	EquationBase* simplified_base = base->_simplify();
	return new Log(simplified_eq, simplified_base);
}

EquationBase* Log::_copy_equation_base() const{
	const Log* casted = dynamic_cast<const Log*>(this);
	return new Log(*casted);
}
void Log::_delete_equation_base(){
	Log* casted = dynamic_cast<Log*>(this);
	delete casted;
}







Ln::Ln(EquationBase* eq) : EquationBase("ln"), eq(eq){
	this->ready_txt = "ln(" + eq->txt() + ")";
}

Ln::Ln(const Ln& lvalue) : EquationBase(lvalue), ready_txt(lvalue.ready_txt){
	const EquationBase* lvalue_eq = lvalue.eq;
	eq = copy(lvalue_eq);
}

Ln::~Ln(){
	delete_equation_base(eq);
}

std::vector<SYMCALC_VAR_NAME_TYPE> Ln::list_variables() const{
	return eq->list_variables();
}


std::string Ln::txt() const{
	return this->ready_txt;
}

SYMCALC_VALUE_TYPE Ln::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::log(eq->eval(var_hash));
}

EquationBase* Ln::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	return new Div(eq->_derivative(var), copy(eq));
}


EquationBase* Ln::_simplify() const{
	EquationBase* simplified = eq->_simplify();
	if(simplified->type == "exp"){
		Exp* casted = dynamic_cast<Exp*>(simplified);
		EquationBase* return_value = copy(casted->eq);
		delete_equation_base(casted);
		return return_value;
	}else if(simplified->type == "const"){
		Constant* casted = dynamic_cast<Constant*>(simplified);
		if(casted->name == "e"){
			delete_equation_base(simplified);
			return new EquationValue(1);
		}
	}
	return new Ln(simplified);
}

EquationBase* Ln::_copy_equation_base() const{
	const Ln* casted = dynamic_cast<const Ln*>(this);
	return new Ln(*casted);
}
void Ln::_delete_equation_base(){
	Ln* casted = dynamic_cast<Ln*>(this);
	delete casted;
}









Exp::Exp(EquationBase* eq) : EquationBase("exp"), eq(eq) {

}

Exp::Exp(const Exp& lvalue) : EquationBase("exp"){
	eq = copy(lvalue.eq);
}



Exp::~Exp(){
	delete_equation_base(eq);
}


std::string Exp::txt() const{
	return std::string("exp(") + eq->txt() + ")";
}


SYMCALC_VALUE_TYPE Exp::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::exp(eq->eval(var_hash));
}

EquationBase* Exp::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	return new Mult({copy(this), eq->_derivative(var)});
}


std::vector<SYMCALC_VAR_NAME_TYPE> Exp::list_variables() const{
	return eq->list_variables();
}


EquationBase* Exp::_simplify() const{
	EquationBase* simplified = eq->_simplify();
	if(simplified->type == "ln"){
		Ln* casted = dynamic_cast<Ln*>(simplified);
		return casted->eq;
	}else{
		return new Exp(simplified);
	}
}

EquationBase* Exp::_copy_equation_base() const{
	const Exp* casted = dynamic_cast<const Exp*>(this);
	return new Exp(*casted);
}
void Exp::_delete_equation_base(){
	Exp* casted = dynamic_cast<Exp*>(this);
	delete casted;
}







Abs::Abs(EquationBase* insides) : EquationBase("abs"), insides(insides) {} // Normal constructor
Abs::Abs(const Abs& lvalue) : EquationBase(lvalue){
	// Use the copy function to copy the insides of the lvalue Abs object
	this->insides = copy(lvalue.insides);
}

// Deconstructor
Abs::~Abs(){
	// Delete the insides on deconstruct
	delete_equation_base(insides);
}


// Dynamic cast copy function
EquationBase* Abs::_copy_equation_base() const{
	const Abs* casted = dynamic_cast<const Abs*>(this);
	return new Abs(*casted); // Create a copy and return it
}


// Dynamic cast delete function
void Abs::_delete_equation_base(){
	Abs* casted = dynamic_cast<Abs*>(this);
	delete casted;
}

// Text represantation
std::string Abs::txt() const{
	return "|" + insides->txt() + "|";
}

// Eval function
SYMCALC_VALUE_TYPE Abs::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	SYMCALC_VALUE_TYPE insides_eval = insides->eval(var_hash);
	if(insides_eval < 0){
		return -insides_eval;
	}else{
		return insides_eval;
	}
}

// List variables function
std::vector<SYMCALC_VAR_NAME_TYPE> Abs::list_variables() const{
	return insides->list_variables();
}

// Derivative function
// |f(x)|' = (f(x) / |f(x)|) * f'(x)
// Example:
// If f(x) = x, then
// |x|' = (x / |x|) * (x)' = (x / |x|) * 1 = x / |x|
EquationBase* Abs::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	EquationBase* insides_derivative = insides->_derivative(var); // f'(x)
	EquationBase* insides_copy_1 = copy(insides); // f(x)
	EquationBase* insides_copy_2 = copy(insides); // f(x)
	EquationBase* abs_insides = new Abs(insides_copy_2); // |f(x)|
	EquationBase* div = new Div(insides_copy_1, abs_insides); // f(x) / |f(x)|
	EquationBase* mult = new Mult({div, insides_derivative}); // (f(x) / |f(x)|) * f'(x)
	return mult;
}


// Simplify function
EquationBase* Abs::_simplify() const{
	EquationBase* simplified_insides = insides->_simplify();
	if(simplified_insides->type == "abs"){
		return simplified_insides; // Absolute function twice is the same as once, ||x|| = |x| 
	}else if(simplified_insides->type == "pow"){
		// Check for a power that can be only positive, e.g. |x^2| = x^2
		const Power* casted = dynamic_cast<const Power*>(simplified_insides);
		if(casted->power->type == "val"){
			const EquationValue* val_casted = dynamic_cast<const EquationValue*>(casted->power);
			if(fmod(val_casted->value, 2) == 0){ // Check if power is divisible by 2
				return simplified_insides;
			}
		}
	}
	// Implement further checks if needed
	
	// If no simplifications work, return just the insides simplified in an abs function
	return new Abs(simplified_insides);
}








Sin::Sin(EquationBase* eq) : EquationBase("sin"), eq(eq) {};

Sin::Sin(const Sin& lvalue) : EquationBase(lvalue){
	eq = copy(lvalue.eq);
}

Sin::~Sin(){
	delete_equation_base(eq);
}


std::string Sin::txt() const{
	return "sin(" + eq->txt() + ")";
}

std::vector<SYMCALC_VAR_NAME_TYPE> Sin::list_variables() const{
	return eq->list_variables();
}


SYMCALC_VALUE_TYPE Sin::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::sin(eq->eval(var_hash));
}


EquationBase* Sin::_simplify() const{
	return copy(this);
}

EquationBase* Sin::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	EquationBase* cos_func = new Cos(copy(eq));
	EquationBase* eq_deriv = eq->_derivative(var);
	return new Mult({cos_func, eq_deriv});
}

EquationBase* Sin::_copy_equation_base() const{
	const Sin* casted = dynamic_cast<const Sin*>(this);
	return new Sin(*casted);
}

void Sin::_delete_equation_base(){
	Sin* casted = dynamic_cast<Sin*>(this);
	delete casted;
}









Cos::Cos(EquationBase* eq) : EquationBase("cos"), eq(eq) {};

Cos::Cos(const Cos& lvalue) : EquationBase(lvalue){
	eq = copy(lvalue.eq);
}

Cos::~Cos(){
	delete_equation_base(eq);
}


std::string Cos::txt() const{
	return "cos(" + eq->txt() + ")";
}

std::vector<SYMCALC_VAR_NAME_TYPE> Cos::list_variables() const{
	return eq->list_variables();
}


SYMCALC_VALUE_TYPE Cos::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return std::cos(eq->eval(var_hash));
}


EquationBase* Cos::_simplify() const{
	return copy(this);
}

EquationBase* Cos::_derivative(SYMCALC_VAR_NAME_TYPE var) const{
	EquationBase* minus_sin_func = new Negate(new Sin(copy(eq)));
	EquationBase* eq_deriv = eq->_derivative(var);
	return new Mult({minus_sin_func, eq_deriv});
}

EquationBase* Cos::_copy_equation_base() const{
	const Cos* casted = dynamic_cast<const Cos*>(this);
	return new Cos(*casted);
}

void Cos::_delete_equation_base(){
	Cos* casted = dynamic_cast<Cos*>(this);
	delete casted;
}







// Constants

namespace Constants{
	Equation Pi ("pi", M_PI);
	Equation E ("e", std::exp(1.0));
}


} // End of symcalc namespace

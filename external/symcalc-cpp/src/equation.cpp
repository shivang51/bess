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
// equation.cpp:
// Definitions for the class Equation and friend operator overloaded functions
//

namespace symcalc{

// Constructors and deconstructors

//
// Rule of Five
// 

// Copy constructor
Equation::Equation(const Equation& other){
	const EquationBase* lvalue_eq = other.eq;	
	eq = copy(lvalue_eq);
}

// Move constructor
Equation::Equation(Equation&& other){
	this->eq = copy(other.eq);
}

// Move assignment
Equation& Equation::operator=(Equation &&other){
	this->eq = copy(other.eq);
	return *this;
}

Equation& Equation::operator=(const Equation& other){
	this->eq = copy(other.eq);
	return *this;
}

// Deconstructor
Equation::~Equation(){
	delete_equation_base(eq);
}


//
// Normal constructors
//

// Constructor for any operation with check to auto simplify or not
Equation::Equation(EquationBase* make_eq) {
	if(make_eq == nullptr)
	throw std::runtime_error("Provided pointer is a nullptr");

	if(SYMCALC_AUTO_SIMPLIFY){
		eq = make_eq->_simplify();
		delete_equation_base(make_eq);
	}else{
		eq = make_eq;
	}
}

// Constructor for wrapped EquationValue
Equation::Equation(SYMCALC_VALUE_TYPE num){
	this->eq = new EquationValue(num);
}

// Constructor for wrapped Variable
Equation::Equation(SYMCALC_VAR_NAME_TYPE var_name) : eq(nullptr){
	eq = new Variable(var_name);
}

// Constructor for wrapped Constant
Equation::Equation(SYMCALC_VAR_NAME_TYPE const_name, SYMCALC_VALUE_TYPE value){
	this->eq = new Constant(const_name, value);
}



// copy.eq() function
EquationBase* Equation::copy_eq() const{
	return copy(this->eq);
}

// type() function
std::string Equation::type() const{
	return eq->type;
}




//
// Friend operators
//

// Print operator for std::cout << Equation
std::ostream& operator<<(std::ostream &stream, const Equation equation){
	stream << equation.eq->txt();
	return stream;
}

// Sum operator for Equation + Equation
Equation operator+(const Equation eq1, const Equation eq2){
	return Equation(new Sum({copy(eq1.eq), copy(eq2.eq)}));
}

// Subtraction operator for Equation - Equation
Equation operator-(const Equation eq1, const Equation eq2){
	return Equation(new Sum({copy(eq1.eq), new Negate(copy(eq2.eq))}));
}

// Multiplication operator for Equation * Equation
Equation operator*(const Equation eq1, const Equation eq2){
	return Equation(new Mult({copy(eq1.eq), copy(eq2.eq)}));
}

// Division operator for Equation / Equation
Equation operator/(const Equation eq1, const Equation eq2){
	return Equation(new Div(copy(eq1.eq), copy(eq2.eq)));
}

// Negate operator for -Equation
Equation operator-(const Equation eq1){
	return Equation(new Negate(copy(eq1.eq)));
}

// Comparison operator for std::map
bool operator<(const Equation holder1, const Equation holder2){
	return true;
}



// Other operators for creating expressions

// Power operator for Equation.pow(Equation)
Equation Equation::pow(Equation power) const{
	EquationBase* copy1 = copy(this->eq);
	EquationBase* copy2 = copy(power.eq);
	return Equation(new Power(copy1, copy2));
}


//
// Abstract operations functions
// Here are functions that relate to SymCalc functionality, like eval and derivative
//


// Derivative functions

Equation Equation::derivative(Equation variable, size_t order) const{
	Variable* var = dynamic_cast<Variable*>(variable.eq);
	if(!var){
		throw std::runtime_error("Provided variable is not of Variable type");
	}
	EquationBase* deriv = eq;
	for(size_t i = 0; i < order; i++){
		deriv = deriv->_derivative(var->name);
	}	
	
	return Equation(deriv);
}


Equation Equation::derivative(size_t order) const{
	std::vector<Equation> vars = this->list_variables();
	if(vars.size() == 1){
		return this->derivative(vars[0], order);
	}else{
		throw std::runtime_error("Equation.derivative expects a variable to differentiate with respect to, since list_variables() gives more than one variable.");
	}
}




// Evaluation functions

SYMCALC_VALUE_TYPE Equation::eval(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return eq->eval(var_hash);
}


SYMCALC_VALUE_TYPE Equation::eval(std::map<Equation, SYMCALC_VALUE_TYPE> var_hash) const{
	SYMCALC_VAR_HASH_TYPE new_var_hash;
	for(std::pair<Equation, SYMCALC_VALUE_TYPE> mypair: var_hash){
		Variable* var = dynamic_cast<Variable*>(mypair.first.eq);
		if(!var){
			throw std::runtime_error("Provided variable is not of Variable type");
		}
		new_var_hash[var->name] = mypair.second;
	}
	return eq->eval(new_var_hash);
}

SYMCALC_VALUE_TYPE Equation::eval() const{
	return this->eval(SYMCALC_VAR_HASH_TYPE());
}

SYMCALC_VALUE_TYPE Equation::operator()(SYMCALC_VAR_HASH_TYPE var_hash) const{
	return eval(var_hash);
}
SYMCALC_VALUE_TYPE Equation::operator()(std::map<Equation, SYMCALC_VALUE_TYPE> var_hash) const{
	return eval(var_hash);
}
SYMCALC_VALUE_TYPE Equation::operator()() const{
	return this->eval(SYMCALC_VAR_HASH_TYPE());
}


// Simplification

Equation Equation::simplify() const{
	return Equation(eq->_simplify());
}




// Listing variables

std::vector<Equation> Equation::list_variables() const{
	std::vector<SYMCALC_VAR_NAME_TYPE> vars = eq->list_variables();
	
	std::vector<Equation> converted;
	
	for(SYMCALC_VAR_NAME_TYPE var : vars){
		converted.push_back(Equation(var));
	}
	
	return converted;	
}

std::vector<std::string> Equation::list_variables_str() const{
	return eq->list_variables();
}


} // End of symcalc namespace



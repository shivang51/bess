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
// helpers.cpp:
// Definitions of functions that help with memory management, logical functions (like include()) and others
//

namespace symcalc{



// Functions that help with properly copying and deleting EquationBase pointers

EquationBase* copy(const EquationBase* start_eq){
	return start_eq->_copy_equation_base();
}

std::vector<EquationBase*> copy(std::vector<const EquationBase*> eqs){
	std::vector<EquationBase*> vec;
	vec.reserve(eqs.size());
	for(const EquationBase* eq : eqs){
		vec.push_back(copy(eq));
	}
	return vec;
}

void delete_equation_base(EquationBase* eq){
	if(eq == nullptr) return;
	
	eq->_delete_equation_base();
	eq = nullptr;
}

	
} // End of symcalc namespace

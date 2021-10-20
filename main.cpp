#include <iostream>
#include <vector>
#include <map>
#include "helpers.hpp"
#include "inputfile.hpp"

using namespace std;


struct InterBasic {
	struct StackFrame { int lno; map<string, Var> vars; };
	struct HeapMemory { VAR_TYPE type; vector<Var> arr; map<string, Var> obj; };
	struct TypeMember { string type, id; };
	struct TypeDef    { string id; vector<TypeMember> members; };

	int      lno = 0;
	int      flag_elseif = 0;
	int32_t  heap_top = 0;

	InputFile                 inp;
	map<string, TypeDef>      types;
	map<string, Var>          vars;
	vector<StackFrame>        callstack;
	map<int32_t, HeapMemory>  heap;


	// debug - show current call stack
	void showenv() const {
		printf(":: env ::\n");
		printf("   :global:\n");
		for (auto& v : vars)
			printf("      %s : %s\n", v.first.c_str(), stringify(v.second).c_str() );
		if (callstack.size()) {
		printf("   :local:\n");
			for (auto& v : callstack.back().vars)
				printf("      %s : %s\n", v.first.c_str(), stringify(v.second).c_str() );
		}
		printf("   :memsize: %02d\n", heap.size());
	}


	// runtime
	void runlines() {
		while (!inp.eof()) {
			runline();
			inp.nextline();
		}
	}
	
	void runline() {
		lno = inp.lno;
		// inp.showtokens(inp.tok, inp.lno);  // debugging!
		auto& cmd = inp.eol() ? inp.peek() : inp.get();
		if      (cmd == "")         ;  // empty line
		else if (is_comment(cmd))   ;  // empty line (with comment)	
		// dim / undim
		else if (cmd == "dim") {
			auto& vv = callstack.size() ? callstack.back().vars : vars;
			while (!inp.eol()) {
				// get type and id info
				string type, id;
				if (is_identifier(inp.peek(0)) && inp.peek(1) == "&" && is_identifier(inp.peek(2)))
					{ type = inp.get();  type += inp.get();  id = inp.get(); }
				else if (is_identifier(inp.peek(0)) && is_identifier(inp.peek(1)))
					{ type = inp.get();  id = inp.get(); }
				else
					{ type = "int";  inp.expecttype("identifier", id); }
				// initialise
				if (type == "int" && inp.peek() == "=")
					{ inp.get();  vv[id] = expr();  if (vv[id].type != VAR_INTEGER) goto _typeerr; }
				else if (type == "int")
					{ vv[id] = Var::ZERO; }
				else if (type == "string" && inp.peek() == "=")
					{ inp.get();  vv[id] = expr();  if (vv[id].type != VAR_STRING) goto _typeerr; }
				else if (type == "string")
					{ vv[id] = Var::EMPTYSTR; }
				else if (type == "array&")
					{ inp.expect("=");  vv[id] = expr();  if (vv[id].type != VAR_ARRAY_REF) goto _typeerr; }
				else if (type == "array")
					{ heap[++heap_top] = { VAR_ARRAY };  vv[id] = { VAR_ARRAY, .i=heap_top }; }
				else if (type == "object&")
					{ inp.expect("=");  vv[id] = expr();  if (vv[id].type != VAR_OBJECT_REF) goto _typeerr; }
				else if (type == "object")
					{ heap[++heap_top] = { VAR_OBJECT };  vv[id] = { VAR_OBJECT, .i=heap_top }; }
				// user type
				// else if (type_defined(type)) {
				// 	auto& o = heap[++heap_top] = { VAR_OBJECT };
				// 	vv[id] = { VAR_OBJECT, .i=heap_top };
				// 	for (auto& m : types[type].members)
				// 		o.obj[m.id] = Var::ZERO;
				// }
				else
					{ throw IBError("unknown type: "+type, lno); }
				// allow comma seperated dim list
				if (inp.peek() != ",")  break;  
				inp.get();
				continue;
				// errors
				_typeerr:  throw IBError("expected expression of type "+type, lno);
			}
			inp.expecttype("eol");
		}
		else if (cmd == "undim") {
			auto& id = inp.get();
			inp.expecttype("eol");
			if      (callstack.size() && callstack.back().vars.count(id))  callstack.back().vars.erase(id);
			else if (vars.count(id))  vars.erase(id);
		}
		else if (cmd == "let") {
			auto& var = expr_varpath_reference();
			if (var.type == VAR_ARRAY || var.type == VAR_OBJECT)  throw IBError("assignment to array / object", lno);
			inp.expect("=");
			auto  val = expr();
			inp.expecttype("eol");
			if (var.type != val.type)  throw IBError("types don't match in assignment", lno);
			var = val;
		}
		// io text commands
		else if (cmd == "print") {
			string s;
			while (!inp.eol()) {
				auto& t = inp.peek();
				if      (t == ",")            printf(" "),   inp.get();  // space seperator
				else if (t == ";")            printf("\t"),  inp.get();  // tab seperator
				else if (is_literal(t, &s))   printf("%s", s.c_str() ),  inp.get();
				// else if (is_identifier(t) || is_integer(t))  printf("%s", stringify(expr()).c_str() );
				// else    throw IBError("print error: ["+t+"]", lno);
				else    printf("%s", stringify(expr()).c_str() );
			}
			printf("\n");
		}
		else if (cmd == "input") {
			auto& var = expr_varpath_reference();
			// if (var.type == VAR_ARRAY || var.type == VAR_OBJECT)  throw IBError("assignment to array / object", lno);
			if (var.type != VAR_STRING)  throw IBError("expected string", lno);
			string s;
			printf("> ");
			getline(cin, s);
			var = { VAR_STRING, .i=0, .s=s };
		}
		// conditionals
		else if (cmd == "if" || cmd == "while") {
			auto& condition = inp.codemap_get(lno);
			auto  v         = expr();
			inp.expecttype("eol");
			if (v.type != VAR_INTEGER)  throw IBError("expected integer", lno);
			if (cmd == "while") {
				if (v.i == 0)  inp.lno = condition.end;
			}
			else if (cmd == "if") {
				if      (v.i)  flag_elseif = 0;  // continue to next line
				else if (v.i == 0 && condition.inner.size())  inp.lno = condition.inner.at(0).pos-1,  flag_elseif = 1;  // goto matching else, set execution to else-mode
				else if (v.i == 0)  inp.lno = condition.end,  flag_elseif = 0;  // goto matching end-if
			}
		}
		else if (cmd == "else") {
			auto& condition = inp.codemap_get(lno);
			if      (!flag_elseif)  inp.lno = condition.end;  // not looking for elses - goto block end
			else if (inp.peek() != "if")  inp.expecttype("eol"),  flag_elseif = 0;  // raw else - continue to next line, and switch to normal execution
			else {
				inp.get();
				auto v = expr();
				inp.expecttype("eol");
				if (v.type != VAR_INTEGER)  throw IBError("expected integer", lno);
				if (v.i)  flag_elseif = 0;  // continue normal execution
				if (!v.i) {
					// find next else, else-if, end-if
					for (int i = 0; i < condition.inner.size(); i++)
						if (condition.inner[i].pos == lno) {
							if    (i+1 < condition.inner.size())  inp.lno = condition.inner[i+1].pos-1;
							else  inp.lno = condition.end;
						}
				}
			}
		}
		// function call
		else if (cmd == "call") {
			auto& id = inp.get();
			// arguments
			vector<Var> args;
			if (inp.peek() == ":") {
				inp.get();
				while (!inp.eol())
					if      (is_comment(inp.peek()))  inp.get();
					else if (inp.peek() == ":")  break;
					else if (inp.peek() == ",")  inp.get();
					// else    frame.vars["_arg"+to_string(++argc)] = expr();
					else    args.push_back( expr() );
			}
			// result
			if (inp.peek() == ":") {
				throw IBError("return paths in call deprecated (for now)", lno);
			}
			inp.expecttype("eol");  // endline
			func_call(id, args);  // do call
		}
		// various jumps
		else if (cmd == "function")  inp.lno = inp.codemap_getfunc(inp.peek()).end;  // skip function block
		else if (cmd == "die")       inp.expecttype("eol"),  inp.lno = inp.lines.size();  // jump to end, and so halt
		else if (cmd == "break")     inp.expecttype("eol"),  inp.lno = inp.codemap_get(lno).end;  // jump to end of while block
		else if (cmd == "continue")  inp.expecttype("eol"),  inp.lno = inp.codemap_get(lno).start;  // jump to start of while block
		else if (cmd == "return")    func_return( inp.eol() ? Var::ZERO : expr() ),  inp.expecttype("eol");  // return from call
		// block end
		else if (cmd == "end") {
			auto &block_type = inp.get();
			inp.expecttype("eol");
			if      (block_type == "if")        flag_elseif = 0;  // make sure we are executing normally
			else if (block_type == "while")     inp.lno = inp.codemap_get(lno).start;
			else if (block_type == "function")  func_return(Var::ZERO);
			else    throw IBError("unknown end: " + block_type, lno);
		}
		// user types
		else if (cmd == "type") {
			string id;
			inp.expecttype("identifier", id);
			inp.expecttype("eol");
			if (type_defined(id))  throw IBError("type redefinition", lno);
			types[id] = { id };
		}
		else if (cmd == "member") {
			string type, mtype, id;
			inp.expecttype("identifier", type);
			inp.expecttype("identifier", mtype);
			if    (inp.eol())  id = mtype,  mtype = "int";
			else  inp.expecttype("identifier", id);
			inp.expecttype("eol");
			if (!types.count(type))        throw IBError("user type not defined: "+type, lno);
			if (!type_defined(mtype))      throw IBError("type not defined", lno);
			if (type == mtype)             throw IBError("circular definition", lno);
			if (type_hasmember(type, id))  throw IBError("type member redefinition", lno);
			types[type].members.push_back({ mtype, id });
		}
		// unknown - error
		else    throw IBError("unknown command: " + cmd, lno);
	}


	// expression parsing
	int flag_expr_eval = 1;
	Var expr() {
		// flag_expr_eval = 1;
		return expr_or();
	}
	Var expr_or() {
		Var  v = expr_and();
		if (inp.peek() == "|" && inp.peek(1) == "|") {
			inp.get(), inp.get();
			if      (!flag_expr_eval)  return expr_or(), Var::VNULL;
			else if (v.type != VAR_INTEGER)  goto _err;
			else if (!v.i) {
				Var q = expr_or();  // note: this should evaluate
				if (q.type != VAR_INTEGER)  goto _err;
				return { VAR_INTEGER, .i = !!q.i };
			}
			else {
				flag_expr_eval = 0;
				expr_or();  // note: this shouldn't evaluate, but needs to for parsing reasons.
				flag_expr_eval = 1;
				return Var::ONE;
			}
		}
		return v;
		_err:   throw IBError("expected integer", lno);
	}
	Var expr_and() {
		Var  v = expr_compare();
		if (inp.peek() == "&" && inp.peek(1) == "&") {
			inp.get(), inp.get();
			if      (!flag_expr_eval)  return expr_and(), Var::VNULL;
			else if (v.type != VAR_INTEGER)  goto _err;
			else if (v.i) {
				Var q = expr_and();  // note: this should evaluate
				if (q.type != VAR_INTEGER)  goto _err;
				return { VAR_INTEGER, .i = !!q.i };
			}
			else {
				flag_expr_eval = 0;
				expr_and();  // note: this shouldn't evaluate, but needs to for parsing reasons.
				flag_expr_eval = 1;
				return Var::ZERO;
			}
		}
		return v;
		_err:   throw IBError("expected integer", lno);
	}
	Var expr_compare() {
		Var  v = expr_add();
		auto cmp = inp.peek();
		if (cmp=="=" || cmp=="!" || cmp==">" || cmp=="<") {
			inp.get();
			cmp += (inp.peek() == "=" ? inp.get() : "");  // note: odd C++ parsing error here
			Var q = expr_add();
			if      (!flag_expr_eval)  return Var::VNULL;
			else if (v.type != VAR_INTEGER || q.type != VAR_INTEGER)  goto _err;
			else if (cmp == "==")  return { VAR_INTEGER, .i = v.i == q.i };
			else if (cmp == "!=")  return { VAR_INTEGER, .i = v.i != q.i };
			else if (cmp == ">")   return { VAR_INTEGER, .i = v.i >  q.i };
			else if (cmp == "<")   return { VAR_INTEGER, .i = v.i <  q.i };
			else if (cmp == ">=")  return { VAR_INTEGER, .i = v.i >= q.i };
			else if (cmp == "<=")  return { VAR_INTEGER, .i = v.i <= q.i };
			// else    goto _err2;
		}
		return v;
		_err:   throw IBError("expected integer", lno);
		// _err2:  throw IBError("bad comparison", lno);
	}
	Var expr_add() {
		Var  v = expr_mul();
		auto op = inp.peek();
		if (op=="+" || op=="-") {
			inp.get();
			Var q = expr_add();
			if      (!flag_expr_eval)  return Var::VNULL;
			else if (v.type != VAR_INTEGER || q.type != VAR_INTEGER)  throw IBError("expected integer", lno);
			else if (op == "+")  return { VAR_INTEGER, .i = v.i + q.i };
			else if (op == "-")  return { VAR_INTEGER, .i = v.i - q.i };
		}
		return v;
	}
	Var expr_mul() {
		Var  v = expr_atom();
		auto op = inp.peek();
		if (op=="*" || op=="/") {
			inp.get();
			Var q = expr_mul();
			if      (!flag_expr_eval)  return Var::VNULL;
			else if (v.type != VAR_INTEGER || q.type != VAR_INTEGER)  throw IBError("expected integer", lno);
			else if (op == "*")  return { VAR_INTEGER, .i = v.i * q.i };
			else if (op == "/")  return { VAR_INTEGER, .i = v.i / q.i };
		}
		return v;
	}
	Var expr_atom() {
		Var v = Var::VNULL;
		string s = inp.peek();
		if      (s == "null")          return inp.get(), v;
		else if (is_identifier(s))     return expr_varpath();
		else if (is_integer(s, &v.i))  return inp.get(), v.type=VAR_INTEGER, v;
		else if (is_literal(s, &v.s))  return inp.get(), v.type=VAR_STRING, v;
		throw IBError("expected Var, got ["+s+"]", lno);
	}
	Var expr_varpath() {
		Var v = expr_varpath_reference();
		if      (v.type == VAR_ARRAY)   v.type = VAR_ARRAY_REF;
		else if (v.type == VAR_OBJECT)  v.type = VAR_OBJECT_REF;
		return v;
	}
	Var& expr_varpath_reference() {
		auto& id = inp.get();
		Var*  v  = &get_def(id);  // first item in chain
		// parse chain
		while (!inp.eol())
			if (inp.peek() == ".") {  // object property
				inp.get();
				auto& id2 = inp.get();
				if (!is_identifier(id2))  goto _err;
				if (!flag_expr_eval)  continue;
				v = &heap_get_at(*v, id2);
			}
			else if (inp.peek() == "[") {  // array offset
				inp.get();
				auto idx = expr();
				if (inp.get() != "]")  goto _err;
				if (!flag_expr_eval)  continue;
				if (idx.type != VAR_INTEGER)  goto _err2;
				v = &heap_get_at(*v, idx.i);
			}
			else  break;
		// end varpath parse
		return *v;
		_err:   throw IBError("expr_varpath", lno);
		_err2:  throw IBError("expected integer", lno);
	}
	// Var expr_call() {
	// 	string id = inp.get();
	// 	inp.expect("(");
	// 	vector<Var> args;
	// 	while (!inp.eof() || inp.peek() != ")") {
	// 		if (args.size())  inp.expect(",");
	// 		args.push_back( expr() );
	// 	}
	// 	inp.expect(")");
	// 	if (!flag_expr_eval)  return Var::VNULL;
	// 	func_call(id, args);
	// 	return vars["_ret"];
	// }



 	// runtime helpers
	Var& get_def(const string& id) {
		if      (!is_identifier(id))  ;
		else if (callstack.size() && callstack.back().vars.count(id))  return callstack.back().vars.at(id);
		else if (vars.count(id))  return vars.at(id);
		throw IBError("undefined variable: "+id, lno);
	}
	HeapMemory& heap_get(const Var& ptr) {
		if (!heap.count(ptr.i))  throw IBError("pointer error", lno);
		return heap.at(ptr.i);
	}
	Var& heap_get_at(const Var& ptr, int32_t offset) {
		auto& mem = heap_get(ptr);
		if (mem.type != VAR_ARRAY || offset < 0 || offset >= mem.arr.size())
			throw IBError("pointer offset error", lno);
		return mem.arr.at(offset);
	}
	Var& heap_get_at(const Var& ptr, const string& property) {
		auto& mem = heap_get(ptr);
		if (mem.type != VAR_OBJECT || !mem.obj.count(property))
			throw IBError("pointer offset error", lno);
		return mem.obj.at(property);
	}
	void heap_free(const Var& ptr) {
		if (ptr.type == VAR_ARRAY || ptr.type == VAR_OBJECT)
			// printf(">>> freeing  %d \n", ptr.i),
			heap.erase(ptr.i);
	}
	void func_call(const string& id, const vector<Var>& args) {
		StackFrame frame = { .lno=inp.lno };
		int argc = 0;
		for (auto& arg : args)
			frame.vars["_arg"+to_string(++argc)] = arg;
		callstack.push_back(frame);
		if    (sysfunc(id))  callstack.pop_back();  // run system function, if possible, then dump local scope
		else  inp.lno = inp.codemap_getfunc(id).start;
	}
	void func_return(const Var& val) {
		if (val.type != VAR_INTEGER)  throw IBError("return value must be integer", lno);
		if (callstack.size() == 0)    throw IBError("no local scope", lno);
		for (auto& vmap : callstack.back().vars)
			heap_free(vmap.second);  // free local vars
		inp.lno = callstack.back().lno;
		callstack.pop_back();
		vars["_ret"] = val;
	}
	int type_defined(const string& type) {
		static const vector<string> DEFAULT_TYPES = { "int", "integer", "string" };
		for (auto& t : DEFAULT_TYPES)
			if (t == type)  return 1;
		if (types.count(type))  return 2;
		return 0;
	}
	int type_hasmember(const string& type, const string& mid) {
		if (!types.count(type))  throw IBError("error", lno);
		for (const auto& m : types[type].members)
			if (m.id == mid)  return 1;
		return 0;
	}
	string stringify(const Var& v) const {
		switch (v.type) {
		case VAR_NULL:     return "null";
		case VAR_INTEGER:  return std::to_string(v.i);
		case VAR_STRING:   return v.s;
		case VAR_ARRAY_REF:
		case VAR_ARRAY:    return "array:" + to_string( v.i );
		case VAR_OBJECT_REF:
		case VAR_OBJECT:   return "object:"+ to_string( v.i );
		}
		throw IBError();
	}



	// runtime system functions
	int sysfunc(const string& id) {
		// array or string length
		if (id == "len") {
			auto& v = get_def("_arg1");
			auto& r = vars["_ret"] = Var::ZERO;
			if      (v.type == VAR_STRING)     r.i = v.s.length();
			else if (v.type == VAR_ARRAY_REF)  r.i = heap_get(v).arr.size();
			else    throw IBError("expected array or string", lno);
		}

		// --- deprecated

		// array position
		else if (id == "at") {
			auto& v = get_def("_arg1");
			auto& p = get_def("_arg2");
			if (v.type != VAR_ARRAY || p.type != VAR_INTEGER)  throw IBError("expected array, integer", lno);
			vars["_ret"] = heap_get_at(v, p.i);
		}
		// make object
		else if (id == "makeobj") {
			heap[++heap_top] = { VAR_OBJECT };
			vars["_ret"] = { VAR_OBJECT, .i=heap_top };
		}
		// make array
		else if (id == "makearray") {
			heap[++heap_top] = { VAR_ARRAY };
			vars["_ret"] = { VAR_ARRAY, .i=heap_top };
		}
		// free memory
		else if (id == "free") {
			auto& v = get_def("_arg1");
			if (v.type != VAR_ARRAY && v.type != VAR_OBJECT)  throw IBError("expected array / object", lno);
			heap.erase(v.i);
			vars["_ret"] = Var::ZERO;
		}

		// ---

		// resize array
		else if (id == "resize") {
			auto& arr  = get_def("_arg1");
			auto& size = get_def("_arg2");
			if (arr.type != VAR_ARRAY_REF || size.type != VAR_INTEGER)  throw IBError("expected array, integer", lno);
			heap_get(arr).arr.resize(size.i);
			vars["_ret"] = Var::ZERO;
		}
		// set object property
		else if (id == "setprop") {
			auto& o = get_def("_arg1");
			auto& p = get_def("_arg2");
			auto& v = get_def("_arg3");
			if (o.type != VAR_OBJECT_REF || p.type != VAR_STRING || v.type == VAR_NULL)
				throw IBError("expected object, string, var", lno);
			auto& obj = heap_get(o).obj;
			obj[p.s] = v;
			vars["_ret"] = v;  // return assugned property
		}
		// string comparison
		else if (id == "strcmp") {
			const auto& a = get_def("_arg1");
			const auto& b = get_def("_arg2");
			if (a.type != VAR_STRING || b.type != VAR_STRING)  throw IBError("expected string, string", lno);
			vars["_ret"] = { VAR_INTEGER, .i = a.s == b.s };
		}
		// split string by whitespace
		else if (id == "split") {
			string s;
			auto& arr_ref   = get_def("_arg1");
			const auto& val = get_def("_arg2");
			if (arr_ref.type != VAR_ARRAY_REF || val.type != VAR_STRING)  throw IBError("expected array, string", lno);
			auto& arr = heap_get(arr_ref).arr;
			for (char c : val.s)
				if (isspace(c)) {
					if (s.size())  arr.push_back({ VAR_STRING, .i=0, .s=s });
					s = "";
				}
				else  s += c;
			if (s.size())  arr.push_back({ VAR_STRING, .i=0, .s=s });
			vars["_ret"] = { VAR_INTEGER, .i=(int32_t)arr.size() };
		}
		else  return 0;
		// found
		return 1;
	}

};


int main() {
	printf("hello world\n");
	InterBasic bas;
	// bas.inp.load("scripts/test.bas");
	bas.inp.load("scripts/advent.bas");
	printf("-----\n");
	//bas.showlines();
	bas.runlines();
	printf("-----\n");
	bas.showenv();
}
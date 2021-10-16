#include <iostream>
#include <vector>
#include <map>
#include "helpers.hpp"
#include "inputfile.hpp"

using namespace std;


struct InterBasic {
	struct StackFrame { int lno; map<string, Var> vars; };
	struct HeapMemory { VAR_TYPE type; vector<Var> arr; map<string, Var> obj; };

	int      lno = 0;
	int      flag_elseif = 0;
	int32_t  heap_top = 0;

	InputFile                 inp;
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
		// inp.showtokens(inp.tok, inp.lno);
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
					{ inp.expect("=");  vv[id] = expr();  if (vv[id].type != VAR_ARRAY) goto _typeerr; }
				else if (type == "array")
					{ heap[++heap_top] = { VAR_ARRAY };  vv[id] = { VAR_ARRAY, .i=heap_top }; }
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
			auto& var = expr_varpath();
			inp.expect("=");
			auto  val = expr();
			inp.expecttype("eol");
			if (var.type == VAR_ARRAY || var.type == VAR_OBJECT)  throw IBError("assignment to array / object", lno);
			// if (var.type == VAR_ARRAY || var.type == VAR_OBJECT || val.type == VAR_ARRAY || val.type == VAR_OBJECT)
			// 	printf("warning: assigning to/from object or array (L%d)\n", lno);
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
			string s;
			auto& v = expr_varpath();
			printf("> ");
			getline(cin, s);
			v = { VAR_STRING, .i=0, .s=s };
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
			auto& id   = inp.get();
			// Var*  res  = NULL;
			int   argc = 0;
			StackFrame frame = { lno };  // new stack frame
			// get arguments
			if (inp.peek() == ":") {
				inp.get();
				while (!inp.eol())
					if      (is_comment(inp.peek()))  inp.get();
					else if (inp.peek() == ":")  break;
					else if (inp.peek() == ",")  inp.get();
					else    frame.vars["_arg"+to_string(++argc)] = expr();
			}
			// put result
			if (inp.peek() == ":") {
				// inp.get();
				// res = &expr_varpath();
				throw IBError("return paths in call deprecated (for now)", lno);
			}
			inp.expecttype("eol");  // endline
			// printf("callstack for %s:\n", id.c_str());
			// for (auto v : frame.vars)
			// 	printf("  %s  %s\n", v.first.c_str(), stringify(v.second).c_str());
			// do call
			callstack.push_back(frame);  // put new frame on callstack
			if    (sysfunc(id))  callstack.pop_back();  // run system function, if possible, then dump local scope
			else  inp.lno = inp.codemap_getfunc(id).start;
			// apply results
			// if (res)  *res = get_def("_ret");
		}
		// various jumps
		else if (cmd == "function")  inp.lno = inp.codemap_getfunc(inp.peek()).end;  // skip function block
		else if (cmd == "die")       inp.expecttype("eol"),  inp.lno = inp.lines.size();  // jump to end, and so halt
		else if (cmd == "break")     inp.expecttype("eol"),  inp.lno = inp.codemap_get(lno).end;  // jump to end of while block
		// else if (cmd == "return")    inp.expecttype("eol"),  inp.lno = callstack.at(callstack.size()-1).lno,  callstack.pop_back();  // return from call
		else if (cmd == "return") {
			vars["_ret"] = inp.eol() ? Var::ZERO : expr();
			if (vars["_ret"].type != VAR_INTEGER)  throw IBError("return value must be integer", lno);
			inp.expecttype("eol");
			if (callstack.size() == 0)  throw IBError("no local scope", lno);
			inp.lno = callstack.back().lno,  callstack.pop_back();
		}
		// block end
		else if (cmd == "end") {
			auto &block_type = inp.get();
			inp.expecttype("eol");
			if      (block_type == "if")        flag_elseif = 0;  // make sure we are executing normally
			else if (block_type == "while")     inp.lno = inp.codemap_get(lno).start;
			else if (block_type == "function") {
				if (callstack.size() == 0)  throw IBError("no local scope", lno);
				inp.lno = callstack.back().lno,  callstack.pop_back();
				vars["_ret"] = Var::ZERO;
			}
			else    throw IBError("unknown end: " + block_type, lno);
		}
		// unknown - error
		else    throw IBError("unknown command: " + cmd, lno);
	}


	// expression parsing
	int flag_expr_eval = 1;
	Var expr() {
		flag_expr_eval = 1;
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
	Var& expr_varpath() {
		auto& id = inp.get();
		Var*  v  = &get_def(id);  // first item in chain
		// parse chain
		while (!inp.eol())
			if (inp.peek() == ".") {  // object property
				inp.get();
				auto& id2 = inp.get();
				if (!is_identifier(id2))  goto _err;
				if (!flag_expr_eval)  continue;
				if (v->type != VAR_OBJECT)  goto _err2;
				if (!heap.count(v->i) || !heap.at(v->i).obj.count(id2))  goto _err3;
				v = &heap.at(v->i).obj.at(id2);
			}
			else if (inp.peek() == "[") {  // array offset
				inp.get();
				auto idx = expr();
				if (inp.get() != "]")  goto _err;
				if (!flag_expr_eval)  continue;
				if (idx.type != VAR_INTEGER)  goto _err2;
				if (!heap.count(v->i) || idx.i < 0 || idx.i >= heap.at(v->i).arr.size())  goto _err3;
				v = &heap.at(v->i).arr.at(idx.i);
			}
			else  break;
		// end varpath parse
		return *v;
		_err:   throw IBError("expr_varpath", lno);
		_err2:  throw IBError("expected integer", lno);
		_err3:  throw IBError("index out of range", lno);
	}



 	// runtime helpers
	Var& get_def(const string& id) {
		if      (!is_identifier(id))  ;
		else if (callstack.size() && callstack.back().vars.count(id))  return callstack.back().vars.at(id);
		else if (vars.count(id))  return vars.at(id);
		throw IBError("undefined variable: "+id, lno);
	}
	// int find_line(const vector<string>& pattern, int start, int direction=1) const {
	// 	int dir = direction < 0 ? -1 : +1;
	// 	for (int i = start; i >= 0 && i < inp.lines.size(); i += dir) {
	// 		auto tok = inp.tokenize(inp.lines.at(i));
	// 		if (tok.size() < pattern.size())  continue;
	// 		for (int j = 0; j < pattern.size(); j++)
	// 			if (tok[j] != pattern[j])  goto _next;
	// 		return i;
	// 		_next:  ;
	// 	}
	// 	throw IBError("find_line: not found: " + join(pattern));
	// }
	// int find_end(const string& type, int start) const {
	// 	int nest = 1;
	// 	for (int i = start; i < inp.lines.size(); i++) {
	// 		auto tok = inp.tokenize(inp.lines.at(i));
	// 		if      (tok.size() < 2)  ;
	// 		else if (tok.at(0) == type)  nest++;
	// 		else if (tok.at(0) == "end" && tok.at(1) == type)  nest--;
	// 		if (nest == 0)  return i;
	// 	}
	// 	throw IBError("find_end: no matching 'end " + type + "'", start);
	// }
	string stringify(const Var& v) const {
		switch (v.type) {
		case VAR_NULL:     return "null";
		case VAR_INTEGER:  return std::to_string(v.i);
		case VAR_STRING:   return v.s;
		case VAR_ARRAY:    return "array:" + to_string( heap.at(v.i).arr.size() );
		case VAR_OBJECT:   return "object:"+ to_string( heap.at(v.i).obj.size() );
		}
		throw IBError();
	}



	// runtime system functions
	int sysfunc(const string& id) {
		// array or string length
		if (id == "len") {
			auto& v = get_def("_arg1");
			auto& r = vars["_ret"] = Var::ZERO;
			if      (v.type == VAR_STRING)  r.i = v.s.length();
			else if (v.type == VAR_ARRAY)   r.i = heap.at(v.i).arr.size();
			else    throw IBError("expected array or string", lno);
		}
		// array position
		// else if (id == "at") {
		// 	auto& v = get_def("_arg1");
		// 	auto& p = get_def("_arg2");
		// 	if (v.type != VAR_ARRAY || p.type != VAR_INTEGER)  throw IBError("expected array, integer", lno);
		// 	vars["_ret"] = heap.at(v.i).arr.at(p.i);
		// }
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
		// resize array
		else if (id == "resize") {
			auto& arr  = get_def("_arg1");
			auto& size = get_def("_arg2");
			if (arr.type != VAR_ARRAY)  throw IBError("expected array", lno);
			heap.at(arr.i).arr.resize(size.i);
			vars["_ret"] = Var::ZERO;
		}
		// free memory
		else if (id == "free") {
			auto& v = get_def("_arg1");
			if (v.type != VAR_ARRAY && v.type != VAR_OBJECT)  throw IBError("expected array / object", lno);
			heap.erase(v.i);
			vars["_ret"] = Var::ZERO;
		}
		// set object property
		else if (id == "setprop") {
			auto& o = get_def("_arg1");
			auto& p = get_def("_arg2");
			auto& v = get_def("_arg3");
			if (o.type != VAR_OBJECT || p.type != VAR_STRING || v.type == VAR_NULL)
				throw IBError("expected object, string, var", lno);
			auto& obj = heap.at(o.i).obj;
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
			if (arr_ref.type != VAR_ARRAY || val.type != VAR_STRING)  throw IBError("expected array, string", lno);
			auto& arr = heap.at(arr_ref.i).arr;
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
#include <iostream>
#include <vector>
#include <map>
#include "helpers.hpp"
#include "inputfile.hpp"

using namespace std;


struct InterBasic {
	InputFile inp;

	struct frame { int lno; map<string, Var> vars; };
	// vector<string> lines, tok;
	// int lno = 0, pos = 0;
	int lno = 0;
	int flag_elseif = 0;

	map<string, Var>          vars;
	vector<frame>             callstack;
	vector<vector<Var>>       heap_arrays;
	vector<map<string, Var>>  heap_objects;


	// asd
	void showenv() const {
		printf("env :: \n");
		for (auto& v : vars)
			printf("	%s : %s\n", v.first.c_str(), stringify(v.second).c_str() );
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
		else if (cmd == "let" || cmd == "local") {
			if (cmd == "local" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = inp.get();
			auto& vv = cmd == "local" ? callstack.back().vars : vars;
			if    (inp.peek() == "=")  inp.get(),  vv[id] = expr();
			else  vv[id] = { VAR_INTEGER, .i=0 };
			inp.expecttype("eol");
		}
		else if (cmd == "unlet" || cmd == "unlocal") {
			if (cmd == "unlocal" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = inp.get();
			auto& vv = cmd == "unlocal" ? callstack.back().vars : vars;
			inp.expecttype("eol");
			if (vv.count(id))  vv.erase(id);  // can unlet missing vars, for simplicity
		}
		// io text commands
		else if (cmd == "print") {
			string s;
			while (!inp.eol()) {
				auto& t = inp.peek();
				if      (t == ",")            printf(" "),   inp.get();  // space seperator
				else if (t == ";")            printf("\t"),  inp.get();  // tab seperator
				else if (is_literal(t, &s))   printf("%s", s.c_str() ),  inp.get();
				else if (is_identifier(t) || is_integer(t))  printf("%s", stringify(expr()).c_str() );
				else    throw IBError("print error: ["+t+"]", lno);
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
			if (v.i == 0) {
				if (condition.inner.size())  inp.lno = condition.inner.at(0).pos-1,  flag_elseif = 1;  // goto matching else
				else  inp.lno = condition.end;  // goto matching end-if
			}
		}
		else if (cmd == "else") {
			auto& condition = inp.codemap_get(lno);
			if      (!flag_elseif)  inp.lno = condition.end;  // not looking for elses - goto block end
			else if (inp.peek() != "if")  inp.expecttype("eol");  // raw else - continue to next line
			else {
				inp.get();
				auto v = expr();
				inp.expecttype("eol");
				if (v.type != VAR_INTEGER)  throw IBError("expected integer", lno);

				// TODO: messy...
				if (v.i) {
					flag_elseif = 0;
				}
				else {
					flag_elseif = 1;
					for (int i = 0; i < condition.inner.size(); i++)
						if (condition.inner[i].pos == lno) {
							if    (i+1 < condition.inner.size())  inp.lno = condition.inner[i+1].pos-1;
							else  inp.lno = condition.end;
						}
				}
				// /messy

			}
		}
		// function call
		else if (cmd == "call") {
			auto& id   = inp.get();
			Var*  res  = NULL;
			int   argc = 0;
			callstack.push_back({ lno });  // new stack frame
			// get arguments
			if (inp.peek() == ":") {
				inp.get();
				while (!inp.eol())
					if      (is_comment(inp.peek()))  inp.get();
					else if (inp.peek() == ":")  break;
					else if (inp.peek() == ",")  inp.get();
					else    callstack.back().vars["_arg"+to_string(++argc)] = expr();
			}
			// put result
			if (inp.peek() == ":") {
				inp.get();
				res = &expr_varpath();
			}
			inp.expecttype("eol");  // endline
			// do call
			if    (sysfunc(id))  callstack.pop_back();  // run system function, if possible, then dump local scope
			else  inp.lno = inp.codemap_getfunc(id).start;
			// apply results
			if (res)  *res = get_def("_ret");
		}
		// various jumps
		else if (cmd == "function")  inp.lno = inp.codemap_getfunc(inp.peek()).end;  // skip function block
		else if (cmd == "die")       inp.expecttype("eol"),  inp.lno = inp.lines.size();  // jump to end, and so halt
		else if (cmd == "break")     inp.expecttype("eol"),  inp.lno = inp.codemap_get(lno).end;  // jump to end of while block
		else if (cmd == "return")    inp.expecttype("eol"),  inp.lno = callstack.at(callstack.size()-1).lno,  callstack.pop_back();  // return from call
		// block end
		else if (cmd == "end") {
			auto &block_type = inp.get();
			inp.expecttype("eol");
			if      (block_type == "if")        flag_elseif = 0;
			else if (block_type == "function")  inp.lno = callstack.at(callstack.size()-1).lno,  callstack.pop_back();
			else if (block_type == "while")     inp.lno = inp.codemap_get(lno).start;
			else    throw IBError("unknown end: " + block_type, lno);
		}
		// unknown - error
		else    throw IBError("unknown command: " + cmd, lno);
	}


	// expression parsing
	//   (kinda messy)
	Var expr() {
		Var  v = expr_atom();
		auto p = inp.peek();
		if (p=="=" || p=="!") {
			// this fucks up on one line... why?
			// string cmp = inp.get() + (inp.peek() == "=" ? inp.get() : "");
			string cmp = inp.get();
			cmp += (inp.peek() == "=" ? inp.get() : "");
			// printf("asd [%s][%s]\n", cmp.c_str(), inp.peek().c_str());
			Var q = expr_atom();
			Var r = { VAR_INTEGER };
			if (v.type != VAR_INTEGER || q.type != VAR_INTEGER)  goto _err;
			if      (cmp == "==")  r.i = v.i == q.i;
			else if (cmp == "!=")  r.i = v.i != q.i;
			else    goto _err;
			return r;
			_err:
			throw IBError("bad comparison", lno);
		}
		return v;
	}
	Var expr_atom() {
		//if (is_identifier(inp.peek()) && inp.peek(1) == "(")  return expr_call();
		//if (is_identifier(inp.peek()))  return get_def(inp.get());
		if (is_identifier(inp.peek()))  return expr_varpath();
		return expr_var(inp.get());
	}
	Var expr_var(const string& s) const {
		Var v = { VAR_NULL };
		if      (s == "null")          return v;
		else if (is_integer(s, &v.i))  return v.type=VAR_INTEGER, v;
		else if (is_literal(s, &v.s))  return v.type=VAR_STRING, v;
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
				if (!is_identifier(id2) || v->type != VAR_OBJECT)  goto _err;
				v = &heap_objects.at(v->i).at(id2);
			}
			else if (inp.peek() == "[") {  // array offset
				inp.get();
				auto idx = expr();
				if (inp.get() != "]" || idx.type != VAR_INTEGER)  goto _err;
				v = &heap_arrays.at(v->i).at(idx.i);
			}
			else  break;
		// end varpath parse
		return *v;
		_err:
		throw IBError("expr_varpath", lno);
	}



 	// runtime helpers
	Var& get_def(const string& id) {
		if      (!is_identifier(id))  ;
		else if (callstack.size() && callstack.back().vars.count(id))  return callstack.back().vars[id];
		else if (vars.count(id))  return vars.at(id);
		throw IBError("undefined variable: "+id);
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
		case VAR_ARRAY:    return "array:" + to_string( heap_arrays .at(v.i).size() );
		case VAR_OBJECT:   return "object:"+ to_string( heap_objects.at(v.i).size() );
		}
		throw IBError();
	}



	// runtime system functions
	int sysfunc(const string& id) {
		// array or string length
		if (id == "len") {
			auto& v = get_def("_arg1");
			auto& r = vars["_ret"] = { VAR_INTEGER, 0 };
			if      (v.type == VAR_STRING)  r.i = v.s.length();
			else if (v.type == VAR_ARRAY)   r.i = heap_arrays.at(v.i).size();
			else    throw IBError("expected array or string", lno);
		}
		// array position
		else if (id == "at") {
			auto& v = get_def("_arg1");
			auto& p = get_def("_arg2");
			if (v.type != VAR_ARRAY || p.type != VAR_INTEGER)  throw IBError("expected array, integer", lno);
			vars["_ret"] = heap_arrays.at(v.i).at(p.i);
		}
		// make object
		else if (id == "make") {
			heap_objects.push_back({});
			vars["_ret"] = { VAR_OBJECT, .i=(int)heap_objects.size()-1 };
		}
		// make array
		else if (id == "makearray") {
			heap_arrays.push_back({});
			vars["_ret"] = { VAR_ARRAY,  .i=(int)heap_arrays.size()-1 };
		}
		// set object property
		else if (id == "setprop") {
			auto& o = get_def("_arg1");
			auto& p = get_def("_arg2");
			auto& v = get_def("_arg3");
			if (o.type != VAR_OBJECT || p.type != VAR_STRING || v.type == VAR_NULL)
				throw IBError("expected object, string, var", lno);
			auto& obj = heap_objects.at(o.i);
			obj[p.s] = v;
			//vars["_ret"] = v;  // return assugned property (pointless?)
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
			auto& v = get_def("_arg1");
			sysfunc("makearray");
			auto& r = vars["_ret"];
			if (v.type != VAR_STRING)  throw IBError("expected string", lno);
			for (auto c : v.s)
				if (isspace(c)) {
					if (s.size())  heap_arrays.at(r.i).push_back({ VAR_STRING, .i=0, .s=s });
					s = "";
				}
				else  s += c;
			if (s.size())  heap_arrays.at(r.i).push_back({ VAR_STRING, .i=0, .s=s });
		}
		else  return 0;
		// found
		return 1;
	}

};


int main() {
	printf("hello world\n");
	InterBasic bas;
	// bas.inp.load("test.bas");
	bas.inp.load("advent.bas");
	printf("-----\n");
	//bas.showlines();
	bas.runlines();
	printf("-----\n");
	bas.showenv();
}
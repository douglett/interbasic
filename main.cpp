#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <map>
#include "helpers.hpp"

using namespace std;



struct InterBasic {
	struct frame { int lno; map<string, Var> vars; };
	vector<string> lines, tok;
	int lno = 0, pos = 0;

	map<string, Var>          vars;
	vector<frame>             callstack;
	vector<vector<Var>>       heap_arrays;
	vector<map<string, Var>>  heap_objects;


	// initialisation
	int load(const string& fname) {
		fstream fs(fname, ios::in);
		if (!fs.is_open())
			return fprintf(stderr, "error loading file %s\n", fname.c_str()), 1;
		string s;
		while (getline(fs, s))
			lines.push_back(s);
		printf("loaded file: %s (%d)\n", fname.c_str(), (int)lines.size());
		return 0;
	}

	vector<string> tokenize(const string& line) const {
		vector<string> vs;
		string s;
		for (int i=0; i<line.length(); i++) {
			char c = line[i];
			if      (isspace(c))  s = s.length() ? (vs.push_back(s), "") : "";
			else if (isalnum(c) || c == '_')  s += c;
			else if (c == '"') {
				s = s.length() ? (vs.push_back(s), "") : "";
				s += c;
				for (++i; i<line.length(); i++) {
					s += line[i];
					if (line[i] == '"')  break;
				}
			}
			else if (c == '#') {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(line.substr(i));
				break;
			}
			else {
				s = s.length() ? (vs.push_back(s), "") : "";
				vs.push_back(string() + c);
			}
		}
		if (s.length())  vs.push_back(s);
		return vs;
	}


	// display
	void showlines() const {
		for (int i=0; i<lines.size(); i++)
			showtokens( tokenize(lines[i]), i );
	}
	void showtokens(const vector<string>& tok, int lno=0) const {
		printf("%02d  ::  ", lno+1);
		for (auto &t : tok)
			printf("[%s]  ", t.c_str());
		printf("\n");
	}
	void showenv() const {
		printf("env :: \n");
		for (auto& v : vars)
			printf("	%s : %s\n", v.first.c_str(), stringify(v.second).c_str() );
	}


	// runtime
	void runlines() {
		for (lno=0; lno<lines.size(); lno++)
			runline(lines[lno]);
	}
	void runline(const string& line) {
		tok = tokenize(line);
		//showtokens(tok, lno);
		auto cmd = tok.size() ? tok.at(0) : "";
		pos = 1;
		if      (cmd == "")         ;  // empty line
		else if (is_comment(cmd))   ;  // empty line (with comment)
		// dim / undim
		else if (cmd == "let" || cmd == "local") {
			if (cmd == "local" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = tok_get();
			auto& vv = cmd == "local" ? callstack.back().vars : vars;
			if    (tok_peek() == "=")  tok_get(),  vv[id] = expr();
			else  vv[id] = { VAR_INTEGER, .i=0 };
		}
		else if (cmd == "unlet" || cmd == "unlocal") {
			if (cmd == "unlocal" && callstack.size() == 0)  throw IBError("no local scope", lno);
			auto& id = tok.at(1);
			auto& vv = cmd == "unlocal" ? callstack.back().vars : vars;
			if (vv.count(id))  vv.erase(id);  // can unlet missing vars, for simplicity
		}
		// print command
		else if (cmd == "print") {
			string s;
			while (pos < tok.size()) {
				auto& t = tok_peek();
				if      (t == ",")            printf(" "),   tok_get();  // space seperator
				else if (t == ";")            printf("\t"),  tok_get();  // tab seperator
				else if (is_literal(t, &s))   printf("%s", s.c_str() ),  tok_get();
				else if (is_identifier(t) || is_integer(t))  printf("%s", stringify(expr()).c_str() );
				else    throw IBError("print error: ["+t+"]", lno);
			}
			printf("\n");
		}
		// ... various
		else if (cmd == "if") {
			auto v = expr();
			if (v.type != VAR_INTEGER)  throw IBError("expected integer");
			if (v.i == 0)               lno = find_end("if", lno+1);  // find matching end-if
		}
		else if (cmd == "call") {
			auto& id   = tok_get();
			Var*  res  = NULL;
			int   argc = 0;
			callstack.push_back({ lno });  // new stack frame
			// get arguments
			if (tok_peek() == ":") {
				tok_get();
				while (pos < tok.size())
					if      (is_comment(tok_peek()))  tok_get();
					else if (tok_peek() == ":")  break;
					else if (tok_peek() == ",")  tok_get();
					else    callstack.back().vars["_arg"+to_string(++argc)] = expr();
			}
			// put result
			if (tok_peek() == ":") {
				tok_get();
				res = &expr_varpath();
			}
			// do call
			if    (sysfunc(id))  ;  // run system function, if possible
			else  lno = find_line({ "function", id }, 0);  // jump to function block definition
			if    (res)  *res = get_def("_ret");
		}
		else if (cmd == "function")  lno = find_line({ "end", "function" }, lno);  // skip function block
		else if (cmd == "die")       lno = lines.size();  // jump to end, and so halt
		else if (cmd == "return") {
			if (callstack.size() == 0)  throw IBError("'return' outside of call");
			lno = callstack.back().lno,  callstack.pop_back();
		}
		else if (cmd == "end") {
			auto &block_type = tok.at(1);
			if      (block_type == "if")        ; // ignore for now
			else if (block_type == "function") {
				if (callstack.size() == 0)  throw IBError("'end function' outside of call");
				lno = callstack.back().lno,  callstack.pop_back();
			}
		}
		else    throw IBError("unknown command: " + cmd, lno);
	}


	// token parsing
	//int    tok_eol () const { return pos >= tok.size() || is_comment(tok[pos]); }
	const string& tok_peek(int ahead=0) const {
		static const string& blank = "";
		return pos+ahead >= tok.size() ? blank : tok[pos+ahead];
	}
	const string& tok_get () {
		if (pos >= tok.size())  throw IBError("expected token", lno);
		return tok[pos++];
	}


	// expression parsing
	//   (kinda messy)
	Var expr() {
		Var  v = expr_atom();
		auto p = tok_peek();
		if (p=="=" || p=="!") {
			string cmp = tok_get() + (tok_peek() == "=" ? tok_get() : "");
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
		//if (is_identifier(tok_peek()) && tok_peek(1) == "(")  return expr_call();
		//if (is_identifier(tok_peek()))  return get_def(tok_get());
		if (is_identifier(tok_peek()))  return expr_varpath();
		return to_var(tok_get());
	}
	Var& expr_varpath() {
		auto& id = tok_get();
		//Var*  v  = NULL;
		//if (!is_identifier(tok_peek()))  goto _err;
		//v = &get_def(id);  // first item in chain
		Var*  v  = &get_def(id);  // first item in chain
		// parse chain
		while (pos < tok.size())
			if (tok_peek() == ".") {  // object property
				tok_get();
				auto& id2 = tok_get();
				if (!is_identifier(id2) || v->type != VAR_OBJECT)  goto _err;
				v = &heap_objects.at(v->i).at(id2);
			}
			else if (tok_peek() == "[") {  // array offset
				tok_get();
				auto idx = expr();
				if (tok_get() != "]" || idx.type != VAR_INTEGER)  goto _err;
				v = &heap_arrays.at(v->i).at(idx.i);
			}
			else  break;
		// end varpath parse
		return *v;
		_err:
		throw IBError("expr_varpath", lno);
	}

	// call in expression
	// TODO: can have side effects, might fuck things
//	Var expr_call() {
//		auto& id = tok_get();
//		auto& cstart = tok_get();
//		if (!is_identifier(id) || !(cstart == "(" || cstart == ":"))  throw IBError();
//
//		while (pos < tok.size())
//			if      (tok_peek() == ")")  break;
//			else if (tok_peek() == ",")  ;
//			else    expr();
//
//		//auto& cend = tok_get();
//		//if (cend != ")")  throw IBError();
//	}

 	// runtime helpers
	Var& get_def(const string& id) {
		if      (!is_identifier(id))  ;
		else if (callstack.size() && callstack.back().vars.count(id))  return callstack.back().vars[id];
		else if (vars.count(id))  return vars.at(id);
		throw IBError("undefined variable: "+id);
	}
	int find_line(const vector<string>& pattern, int start) const {
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i]);
			if (tok.size() < pattern.size())  continue;
			for (int j=0; j<pattern.size(); j++)
				if (tok[j] != pattern[j])  goto _next;
			return i;
			_next:  ;
		}
		throw IBError("find_line: not found: " + join(pattern));
	}
	int find_end(const string& type, int start) const {
		int nest = 1;
		for (int i=start; i<lines.size(); i++) {
			auto tok = tokenize(lines[i]);
			if      (tok.size() < 2)  ;
			else if (tok.at(0) == type)  nest++;
			else if (tok.at(0) == "end" && tok.at(1) == type)  nest--;
			if (nest == 0)  return i;
		}
		throw IBError("find_end: no matching 'end " + type + "'", start);
	}
	Var to_var(const string& s) const {
		Var v = { VAR_NULL };
		if      (s == "null")          return v;
		else if (is_integer(s, &v.i))  return v.type=VAR_INTEGER, v;
		else if (is_literal(s, &v.s))  return v.type=VAR_STRING, v;
		throw IBError("expected Var, got ["+s+"]", lno);
	}
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
		// split string by whitespace
		else if (id == "split") {
			string s;
			auto& v = get_def("_arg1");
			sysfunc("makearray");
			auto& r = vars["_ret"];
			if (v.type != VAR_STRING)  throw IBError("expected string", lno);
			for (auto c : v.s)
				if (isspace(c)) {
					if (s.size())  heap_arrays.at(r.i).push_back({ VAR_STRING, .s=s });
					s = "";
				}
				else  s += c;
			if (s.size())  heap_arrays.at(r.i).push_back({ VAR_STRING, .s=s });
		}
		else  return 0;
		return 1;
	}

};


int main() {
	printf("hello world\n");
	InterBasic bas;
	bas.load("test.bas");
	printf("-----\n");
	//bas.showlines();
	bas.runlines();
	printf("-----\n");
	bas.showenv();
}
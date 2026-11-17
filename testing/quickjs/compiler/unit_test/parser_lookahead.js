// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

function check(source, expected) {
  const actual = (0, eval)(source);
  if (actual !== expected) {
    throw new Error(source + ": expected " + expected + ", got " + actual);
  }
}

function syntaxError(source) {
  try {
    (0, eval)(source);
  } catch (error) {
    if (error instanceof SyntaxError) return;
    throw error;
  }
  throw new Error("Expected SyntaxError for " + source);
}

check("(1 + 2)", 3);
check("(1 + 2)\r\n;", 3);
check("([{x: [3, {text: ')],}'}]}])[0].x[0]", 3);
check("({text: 'a\\'b\\\\c'}).text", "a'b\\c");
check("((x) => x + 1)(2)", 3);
check("((x) /* before arrow */ => x + 1)(2)", 3);
check("((x = 3) => x)()", 3);
check("(() => { let x; [x] /* before assign */ = [3]; return x; })()", 3);
check("(() => { let x; ({x} = {x: 3}); return x; })()", 3);
check("(() => { let x, rest; [x, ...rest] = [1, 2, 3]; return x + rest.length; })()", 3);
check("([6 / 2, /[)\\]]/.test(']')])[0]", 3);
check("([1 /* ] */ + 2])[0]", 3);
check("({text: `x${1 + 2}]`}).text", "x3]");
check("({caf\u00e9: '\\u0041'}).caf\u00e9", "A");
check("({\\u0078: 3}).x", 3);
check("({x: 3})\u00a0.x", 3);
check("(function(){ return [1, 2, 3]; })().length", 3);
check("(".repeat(70) + "3" + ")".repeat(70), 3);
check("({x: 3})<!-- }\n.x", 3);
syntaxError("(x)\n=> x");
syntaxError("([)]");
syntaxError("({x: 'unterminated})");
syntaxError("(1 + 2) = 3");
syntaxError("[1, 2] = [3, 4]");

// Lookahead must not move identifier locations to a later closing delimiter.
function checkReferenceColumn(body, column) {
  const source = "function f(){\n  " + body + "\n}\nf();";
  try {
    (0, eval)(source);
  } catch (error) {
    if (error instanceof ReferenceError &&
        error.stack.includes("at f (<input>:2:" + column + ")")) {
      return;
    }
    throw error;
  }
  throw new Error("Expected ReferenceError at column " + column);
}

checkReferenceColumn("return (((missingValue))) + (5);", 13);
checkReferenceColumn("return ({x: [1, 2, innermost]});", 22);

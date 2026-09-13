#!/usr/bin/env node
"use strict";

function greet(name = "world") {
  return `Hello, ${name}!`;
}

const args = process.argv.slice(2);
const who = args.length > 0 ? args : ["world"];

for (const name of who) {
  console.log(greet(name));
}

module.exports = { greet };

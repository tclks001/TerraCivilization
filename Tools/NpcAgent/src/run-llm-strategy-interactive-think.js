#!/usr/bin/env node

// Reuse the proven interactive agent while enabling LLM-selected strategic queries.
for (const flag of ["--allow-strategy-tools", "--no-strategy-prefetch", "--llm-tool-takeaways"]) {
  if (!process.argv.includes(flag)) {
    process.argv.push(flag);
  }
}

await import("./run-llm-interactive-think.js");

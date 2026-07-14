#!/usr/bin/env node

// Expose current Gameplay UI tools, strategic queries, and local topology observation to the LLM.
for (const flag of ["--allow-strategy-tools", "--allow-topology-tool", "--no-strategy-prefetch", "--llm-tool-takeaways"]) {
  if (!process.argv.includes(flag)) {
    process.argv.push(flag);
  }
}

await import("./run-llm-interactive-think.js");

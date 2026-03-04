## C-AMM: Constant Product Market Maker Simulator
A minimalist, high-performance simulation of an Automated Market Maker (AMM) using the Uniswap v2 algorithm, written in pure C.

### The Concept

This project explores the financial math behind Liquidity Pools without the abstraction of Smart Contracts. By implementing the constant product formula in C, I demonstrate how token prices, slippage, and fee accumulation work at the protocol level.

It uses the Constant Product Invariant: x * y = k

### Key Features

Universal Swap Logic: A direction-agnostic swap function (A to B or B to A) using C pointers.

Fee Accumulation: Simulates a 0.3% fee that remains in the pool, increasing the invariant (k) over time.

Price Discovery: Calculates the Spot Price vs. Execution Price to show real-time slippage.

Risk Analytics: Built-in Impermanent Loss (IL) calculator comparing Pool Value against a HODL Strategy.

Safety Guards: Protection against division by zero and reserve depletion.

### How to Run
Ensure you have a C compiler (like GCC) installed.

1. Clone the repository

2. Compile the code: gcc main.c -o amm_sim

2. Run the executable: ./amm_sim

### Roadmap
[ ] Implement mint() and burn() for Liquidity Providers.

[ ] Add LP Token tracking to handle pool ownership shares.

[ ] Export simulation data to CSV for price curve visualization.
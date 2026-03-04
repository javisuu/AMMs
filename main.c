#include <stdio.h>

typedef struct {
    double token_reserves_A;
    double token_reserves_B;

}LiquidityPool;

typedef enum {TOKEN_A,TOKEN_B} TokenType;

// k is invariant
double calculate_k(LiquidityPool pool) {
    return pool.token_reserves_A * pool.token_reserves_B;
}
//

void swap(LiquidityPool *pool, double amount_in, TokenType input_type) {

    double *res_in; //What I introduce to the pool
    double *res_out; //What I extract from the pool

    if (input_type == TOKEN_A) {
        res_in=&pool->token_reserves_A;
        res_out=&pool->token_reserves_B;
    }else {
        res_in=&pool->token_reserves_B;
        res_out=&pool->token_reserves_A;
    }

    // 0. A fee
    double fee_percent= 0.003;
    double amount_in_with_fee= amount_in * (1-fee_percent);
    // 1. Calculate amount_out using the formula above
    if (*res_in<=0) {
        fprintf(stderr,"Division by zero\n");
        return;
    }
    double amount_out= (*res_out * amount_in_with_fee) / (*res_in + amount_in_with_fee);
    // If the amount of the tokens extracted is greater than the reserves of that token in a moment t,
    // the transaction should be aborted
    if (amount_out>=*res_out) { //Reserves may not hit 0 so is greater or equal
        fprintf(stderr,"TRANSACTION ABORTED - Amount is greater than the reserves of the pool\n");
        return;
    }

    // 2. Update pool->token_a_reserves
    *res_in += amount_in; // We add the amount_in without extracting the fee rate

    // 3. Update pool->token_b_reserves
    *res_out -= amount_out;
    // 4. Print the results!
    printf("Swapped %.2f %s for %.2f %s\n",
            amount_in, (input_type == TOKEN_A ? "A" : "B"),
            amount_out, (input_type == TOKEN_A ? "B" : "A"));
}

void main(void) {
    double initialA= 100;
    double initialB= 100;

    LiquidityPool pool;
    pool.token_reserves_A = initialA;
    pool.token_reserves_B = initialB;
    int i;
    double result = calculate_k(pool);
    printf("The pool is open: %.2lf\n",result);
    /*for (i=0;i<10;i++) {
        swap(&pool,10,TOKEN_A);
    }
    printf("After 10 SWAPS - \n");
    printf("POOL - New A: %.2lf| New B: %.2lf | New k: %.2lf\n",pool.token_reserves_A,pool.token_reserves_B,calculate_k(pool));
    */
    swap(&pool, 50, TOKEN_A);
    swap(&pool, 50, TOKEN_B);

    // --- IMPERMANENT LOSS CALCULATION ---

    // A. What is the price of A in terms of B right now?
    double current_price_A = pool.token_reserves_B/pool.token_reserves_A;

    // B. HODL Value: What if we just kept the 100A and 100B in a wallet?
    // Value = (Amount of A * Current Price of A) + (Amount of B)
    double value_Hodl = initialA * current_price_A + initialB;

    // C. Pool Value: What is our share of the pool worth now?
    // Value = (Current Reserves A * Current Price A) + (Current Reserves B)
    double value_Pool= pool.token_reserves_A * current_price_A + pool.token_reserves_B;

    // D. Calculate the difference
    double il_percentage= ((value_Hodl - value_Pool)/ value_Hodl) * 100;

    printf("\n--- Final Analysis ---\n");
    printf("Value if you just HODL'd: %.2lf\n", value_Hodl);
    printf("Value of Pool Holdings:   %.2lf\n", value_Pool);
    printf("Net Profit/Loss vs HODL:  %.2lf%%\n", il_percentage);


}
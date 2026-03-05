#include <stdio.h>

typedef struct {
    double token_reserves_A;
    double token_reserves_B;
    double total_LP_shares;

}LiquidityPool;

typedef enum {TOKEN_A,TOKEN_B} TokenType;

typedef struct {
    double wallet_A;
    double wallet_B;
    double lp_shares;
}User;

typedef struct {
    double amount_A;
    double amount_B;
}Withdrawal;

// k is invariant
double calculate_k(LiquidityPool pool) {
    return pool.token_reserves_A * pool.token_reserves_B;
}
//

double swap(LiquidityPool *pool, double amount_in, TokenType input_type) {

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
    return amount_out;
}

//Ads liquity
double mint(LiquidityPool *pool, double amount_A) {
    double amount_B;
    double new_shares;
    // 1. Adding shares
    // If the pool is empty, it should be initialized
    if (pool->total_LP_shares==0) {
        amount_B=amount_A; // 1:1 as default
        new_shares=amount_A;

    }else {
        //2. Deposit in B depends on A
        amount_B= amount_A*(pool->token_reserves_B/pool->token_reserves_A);

        //3. New shares
        new_shares=(amount_A/pool->token_reserves_A)*pool->total_LP_shares;
    }
    //UPDATE the pool
    pool->token_reserves_A+=amount_A;
    pool->token_reserves_B+=amount_B;
    pool->total_LP_shares+=new_shares;

    printf(">>> MINT: Pool received %.2f A and %.2f B. Total Shares now: %.2f\n",
            amount_A, amount_B, pool->total_LP_shares);
    return new_shares;
}

Withdrawal burn(LiquidityPool *pool, double shares_to_burn) {

    double fraction;
    //1. Calculate the fraction
    fraction=shares_to_burn/pool->total_LP_shares;

    //2. Calculate the amounts to payback
    double amount_A_fraction=fraction*pool->token_reserves_A;
    double amount_B_fraction=fraction*pool->token_reserves_B;

    //SUBSTRACT from the pool
    pool->token_reserves_A-=amount_A_fraction;
    pool->token_reserves_B-=amount_B_fraction;
    pool->total_LP_shares-=shares_to_burn;

    printf("<<< BURN: Destroyed %.2f shares. Returned %.2f A and %.2f B.\n",
            shares_to_burn, amount_A_fraction, amount_B_fraction);
    Withdrawal result={amount_A_fraction,amount_B_fraction};
    return result;

}

// Wrap functions to update user's wallet

void execute_mint(User *user, LiquidityPool *pool, double amount_A) {
    double required_B;
    // 1. Calculate required B based on pool ratio
    if (pool->total_LP_shares == 0) {
        // For the very first deposit, we assume a 1:1 ratio
        // OR you can pass a specific amount_B as a parameter.
        required_B = amount_A;
    } else {
        double ratio = pool->token_reserves_B / pool->token_reserves_A;
        required_B = amount_A * ratio;
    }

    // 2. Pre-flight Checks (The Safety Guard)
    if (user->wallet_A < amount_A || user->wallet_B < required_B) {
        printf("FAILED: User has insufficient funds. (Need %.2f B, Have %.2f B)\n",
                required_B, user->wallet_B);
        return;
    }

    // 3. Update User Wallet (Atomic with the protocol call)
    user->wallet_A -= amount_A;
    user->wallet_B -= required_B;

    // 4. Call the Core Protocol & capture issued shares
    double shares_received = mint(pool, amount_A);
    user->lp_shares += shares_received;

    printf("SUCCESS: Minted %.2f shares for %s\n", shares_received, "User");
}

void execute_burn(User *user, LiquidityPool *pool, double shares) {
    // Safety check BEFORE calling the protocol
    if (user->lp_shares < shares) {
        printf("FAILED: You don't own enough shares!\n");
        return;
    }

    // Call the protocol and capture the returned tokens
    Withdrawal received = burn(pool, shares);

    // Update User Wallet with the returned amounts
    user->wallet_A += received.amount_A;
    user->wallet_B += received.amount_B;
    user->lp_shares -= shares;

    printf("SUCCESS: Received %.2f A and %.2f B\n", received.amount_A, received.amount_B);
}

void excute_swap(User *user, LiquidityPool *pool,double amount_in,TokenType input_type ) {
    double *spend_wallet;
    double *receive_wallet;
    // 1. Identify the wallet the user is spending for
    if (input_type == TOKEN_A) {
        spend_wallet=&user->wallet_A;
        receive_wallet=&user->wallet_B;
    }else {
        spend_wallet=&user->wallet_B;
        receive_wallet=&user->wallet_A;

    }
    // 2. Verify the viability of the transaction
    if (amount_in>*spend_wallet) {
        printf("FAILED: User has insufficient %s balance!\n",
                (input_type == TOKEN_A ? "A" : "B"));
        return;
    }
    //3. Deduct from the wallet
    *spend_wallet-=amount_in;

    //4. Execute the swap
    double amount_received=swap(pool, amount_in, input_type);

    //5. User out
    *receive_wallet+=amount_received;

    printf("SUCCESS: User wallet updated (+%.2f tokens).\n", amount_received);

}

void main(void) {

    // 1. Set up the pool
    LiquidityPool pool={0,0,0};

    // 2. Set up the user
    User me={500,500,0};

    // 3. Deposit amount
    // Clean, modular execution
    execute_mint(&me, &pool, 50);

    // Check state
    printf("Wallet A: %.2f | Wallet B: %.2f | Shares: %.2f\n",
            me.wallet_A, me.wallet_B, me.lp_shares);
    //Total shares get
    /*me.lp_shares = pool.total_LP_shares;

    printf("User now has %.2f LP shares and %.2f A in wallet.\n", me.lp_shares, me.wallet_A);

    burn(&pool,100);

    printf("User now has %.2f LP shares and %.2f A in wallet.\n", me.lp_shares, me.wallet_A);

    /*int i;
    double result = calculate_k(pool);
    printf("The pool is open: %.2lf\n",result);
    for (i=0;i<10;i++) {
        swap(&pool,10,TOKEN_A);
    }
    printf("After 10 SWAPS - \n");
    printf("POOL - New A: %.2lf| New B: %.2lf | New k: %.2lf\n",pool.token_reserves_A,pool.token_reserves_B,calculate_k(pool));

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
    printf("Net Profit/Loss vs HODL:  %.2lf%%\n", il_percentage);*/


}
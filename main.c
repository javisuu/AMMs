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
//HELPER FUNCTION - CALCULATE THE AMOUNT OUT
double get_amount_out(LiquidityPool pool, double amount_in, TokenType input_type) {
    double res_in = (input_type == TOKEN_A) ? pool.token_reserves_A : pool.token_reserves_B;
    double res_out = (input_type == TOKEN_A) ? pool.token_reserves_B : pool.token_reserves_A;

    double fee_percent = 0.003;
    double amount_in_with_fee = amount_in * (1 - fee_percent);
    if (res_in<=0) {
        fprintf(stderr,"Division by zero\n");
        return;
    }

    // The Standard xy=k Formula
    return (res_out * amount_in_with_fee) / (res_in + amount_in_with_fee);
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

    double amount_out= get_amount_out(*pool, amount_in, input_type);
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
double mint(LiquidityPool *pool, double amount_A,double amount_B) {
    double new_shares;
    // 1. Adding shares
    // If the pool is empty, it should be initialized
    if (pool->total_LP_shares == 0) {
        // Initial liquidity: User defines the starting ratio
        new_shares = amount_A;
    } else {
        // We use A as the reference, but since the ratio is forced,
        // using B would give the exact same answer.
        // Standard Uniswap V2 logic:
        // new_shares = total_shares * (deposited_amount / existing_reserve)
        double shares_A = (amount_A / pool->token_reserves_A) * pool->total_LP_shares;
        double shares_B = (amount_B / pool->token_reserves_B) * pool->total_LP_shares;
        new_shares = (shares_A<shares_B)?shares_A:shares_B;
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

void execute_mint(User *user, LiquidityPool *pool, double amount, TokenType lead_token) {
    double deposit_A;
    double deposit_B;
    // 1. Calculate required B based on pool ratio
    if (pool->total_LP_shares == 0) {
        // For the very first deposit, we assume a 1:1 ratio
        // OR you can pass a specific amount_B as a parameter.
        deposit_A = amount;
        deposit_B = amount;
    } else {
        if (lead_token == TOKEN_A) {
            deposit_A=amount;
            deposit_B=amount * (pool->token_reserves_B/pool->token_reserves_A );
        }else {
            deposit_B=amount;
            deposit_A=amount*(pool->token_reserves_A/pool->token_reserves_B);

        }

    }

    // 2. Pre-flight Checks (The Safety Guard)
    if (user->wallet_A < deposit_A || user->wallet_B < deposit_B) {
        printf("FAILED: User has insufficient funds. (Need %.2f A and %.2f B)\n", deposit_A, deposit_B);
        return;
    }

    // 3. Update User Wallet (Atomic with the protocol call)
    user->wallet_A -= deposit_A;
    user->wallet_B -= deposit_B;

    // 4. Call the Core Protocol & capture issued shares
    double shares_received = mint(pool, deposit_A, deposit_B);
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

void execute_swap(User *user, LiquidityPool *pool,double amount_in,TokenType input_type, double min_amount_out ) {
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

    double amount_out=get_amount_out(*pool, amount_in, input_type);

    if (amount_out<min_amount_out) {
        printf("FAILED: Slippage too high! (Expected >%.2f, Got %.2f)\n", min_amount_out, amount_out);
        //ABORTED - UNDO

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

    // 1. Setup
    LiquidityPool pool = {0, 0, 0};
    User provider = {1000, 1000, 0}; // The Liquidity Provider (LP)
    User trader = {500, 500, 0};     // The Trader

    printf("--- INITIAL STATE ---\n");
    printf("Provider Wallet: A: %.2f, B: %.2f\n\n", provider.wallet_A, provider.wallet_B);

    // 2. Provision (LP adds 100A and 100B)
    printf("--- LP DEPOSIT ---\n");
    execute_mint(&provider, &pool, 100, TOKEN_A);

    // 3. Trading (Trader performs a large swap)
    printf("\n--- TRADER SWAPS ---\n");
    // Trader wants to swap 50 A for at least 40 B (Slippage guard)
    execute_swap(&trader, &pool, 50, TOKEN_A, 40.0);

    // 4. Reverse Swap (Price moves back)
    // Trader swaps some B back to A to rebalance the pool
    execute_swap(&trader, &pool, 20, TOKEN_B, 15.0);

    // 5. Withdrawal (LP takes their money back)
    printf("\n--- LP WITHDRAWAL ---\n");
    execute_burn(&provider, &pool, provider.lp_shares);

    // 6. Final Result
    printf("\n--- FINAL ANALYSIS ---\n");
    printf("Provider Final Wallet: A: %.2f, B: %.2f\n", provider.wallet_A, provider.wallet_B);

    double total_start = 2000.0; // 1000 + 1000
    double total_end = provider.wallet_A + provider.wallet_B;
    printf("Net Profit from Fees: %.4f tokens\n", total_end - total_start);


}
The following steps must be taken for the example script to work.

0. Create wallet
0. Create account for celes.token
0. Create account for scott
0. Create account for exchange
0. Set token contract on celes.token
0. Create EOS token
0. Issue initial tokens to scott

**Note**:
Deleting the `transactions.txt` file will prevent replay from working.


### Create wallet
`cleos wallet create`

### Create account steps
`cleos create key`

`cleos create key`

`cleos wallet import  --private-key <private key from step 1>`

`cleos wallet import  --private-key <private key from step 2>`

`cleos create account eosio <account_name> <public key from step 1> <public key from step 2>`

### Set contract steps
`cleos set contract celes.token /contracts/celes.token -p celes.token@active`

### Create EOS token steps
`cleos push action celes.token create '{"issuer": "celes.token", "maximum_supply": "100000.0000 EOS", "can_freeze": 1, "can_recall": 1, "can_whitelist": 1}' -p celes.token@active`

### Issue token steps
`cleos push action celes.token issue '{"to": "scott", "quantity": "900.0000 EOS", "memo": "testing"}' -p celes.token@active`

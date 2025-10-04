#!/bin/bash
for i in {1..5}; do
  ./client "Mensagem do cliente $i" &
done
wait

elementclass RateControl {
  $rate, $rates|

  rate_control :: Minstrel(OFFSET 4, RT $rates);

  input -> rate_control -> output;
  input [1] -> [1] rate_control;

};

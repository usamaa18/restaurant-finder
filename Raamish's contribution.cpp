// this function will partition and rearrange the array for quick sort
int partition(RestDist restaurants[], int n, int pi){
  swap(restaurants[pi], restaurants[n-1]);
  
  int low_end = 0;
  int high_end = n - 2;

  while (high_end >= low_end){
    if (restaurants[low_end].dist <= restaurants[n - 1].dist){
      low_end = low_end + 1;
    }
    else if (restaurants[high_end].dist > restaurants[n - 1].dist){
      high_end = high_end - 1;
    }
    else {
      swap(restaurants[low_end], restaurants[high_end]);
    }
  }
  swap(restaurants[low_end], restaurants[n - 1]);
  return low_end;
}


// Quick sort
void qsort(RestDist restaurants[], int n){
  if (n <= 1){
    return;
  }
  // select a pivot position
  int newPartition = partition(restaurants, n, ((n-1)/2));
  qsort(restaurants, newPartition);
  qsort(restaurants + newPartition + 1, n - newPartition -1);
}


//////////////////


  uint32_t StartTimer_quicksort = millis();
  quicksort();
  uint32_t EndTimer_quicksort = millis();
  total_quicksort = EndTimer_quicksort - StartTimer_quicksort;

  Serial.println("quicksort running time: ");
  Serial.println(total_quicksort);
  Serial.println(" ms");
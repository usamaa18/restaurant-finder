#include "restaurant.h"

/*
	Sets *ptr to the i'th restaurant. If this restaurant is already in the cache,
	it just copies it directly from the cache to *ptr. Otherwise, it fetches
	the block containing the i'th restaurant and stores it in the cache before
	setting *ptr to it.
*/
void getRestaurant(restaurant* ptr, int i, Sd2Card* card, RestCache* cache) {
	// calculate the block with the i'th restaurant
	uint32_t block = REST_START_BLOCK + i/8;

	// if this is not the cached block, read the block from the card
	if (block != cache->cachedBlock) {
		while (!card->readBlock(block, (uint8_t*) cache->block)) {
			Serial.print("readblock failed, try again");
		}
		cache->cachedBlock = block;
	}

	// either way, we have the correct block so just get the restaurant
	*ptr = cache->block[i%8];
}

// Swaps the two restaurants (which is why they are pass by reference).
void swap(RestDist& r1, RestDist& r2) {
	RestDist tmp = r1;
	r1 = r2;
	r2 = tmp;
}

// Insertion sort to sort the restaurants.
void insertionSort(RestDist restaurants[], int size) {
	// Invariant: at the start of iteration i, the
	// array restaurants[0 .. i-1] is sorted.
	for (int i = 1; i < size; ++i) {
		// Swap restaurant[i] back through the sorted list restaurants[0 .. i-1]
		// until it finds its place.
		for (int j = i; j > 0 && restaurants[j].dist < restaurants[j-1].dist; --j) {
			swap(restaurants[j-1], restaurants[j]);
		}
	}
}

// Computes the manhattan distance between two points (x1, y1) and (x2, y2).
int16_t manhattan(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	return abs(x1-x2) + abs(y1-y2);
}


// partitioning and rearranging the array for quick sort
int partition(RestDist restaurants[], int n, int pi){
  //swapping pivot index to the last element
  swap(restaurants[pi], restaurants[n-1]);
  
  //settuing low and high values
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
  
  //return the new location
  return low_end;
}


// Quick sort
void qSort(RestDist restaurants[], int n){
  if (n <= 1){
    return;
  }
  // finding a new partition using a new pivot position
  int newPartition = partition(restaurants, n, ((n-1)/2));
  
  //calling the function for the 2 halves of the array using recursion
  qSort(restaurants, newPartition);
  qSort(restaurants + newPartition + 1, n - newPartition -1);
}



/*
	Fetches all restaurants from the card, saves their RestDist information
	in restaurants[], and then sorts them based on their distance to the
	point on the map represented by the MapView. Returns length of restaurants array.
*/
int getAndSortRestaurants(const MapView& mv, RestDist restaurants[], Sd2Card* card, RestCache* cache, int minRating, int mode) {
	restaurant r;

	int nRest = 0;

	// First get all the restaurants and store their corresponding RestDist information.
	for (int i = 0; i < NUM_RESTAURANTS; ++i) {
		getRestaurant(&r, i, card, cache);
		int rating = max(floor((r.rating+1)/2),1);
		if (rating>=minRating){
			restaurants[nRest].index = i;
			restaurants[nRest].dist = manhattan(lat_to_y(r.lat), lon_to_x(r.lon),
																			mv.mapY + mv.cursorY, mv.mapX + mv.cursorX);
			nRest++;
		}
	}

	// Now sort them based on selected mode

	// quicksort
	if (mode == 0){
		uint32_t StartTimer = millis();
		qSort(restaurants,nRest);
	  	uint32_t EndTimer = millis();
	  	uint32_t total = EndTimer - StartTimer;

	  	Serial.print("Quicksort running time: ");
	  	Serial.print(total);
	  	Serial.print(" ms");
	  	Serial.println();
	}
	//insertion sort
	else if (mode == 1){
		uint32_t StartTimer = millis();
		insertionSort(restaurants,nRest);
	  	uint32_t EndTimer = millis();
	  	uint32_t total = EndTimer - StartTimer;

	  	Serial.print("Insertion sort running time: ");
	  	Serial.print(total);
	  	Serial.print(" ms");
	  	Serial.println();
		
	}
	// both
	else {
/*
		RestDist temp[nRest];
		for (int i = 0; i < nRest; i++)
			temp[i] = restaurants[i];
*/
		uint32_t StartTimer = millis();
		qSort(restaurants,nRest);
	  	uint32_t EndTimer = millis();
	  	uint32_t total = EndTimer - StartTimer;

	  	Serial.print("Quicksort running time: ");
	  	Serial.print(total);
	  	Serial.print(" ms");
	  	Serial.println();

	  	int nRest = 0;

		// reload from SD card. As directed in the document on eClass
		for (int i = 0; i < NUM_RESTAURANTS; ++i) {
			getRestaurant(&r, i, card, cache);
			int rating = max(floor((r.rating+1)/2),1);
			if (rating>=minRating){
				restaurants[nRest].index = i;
				restaurants[nRest].dist = manhattan(lat_to_y(r.lat), lon_to_x(r.lon),
																				mv.mapY + mv.cursorY, mv.mapX + mv.cursorX);
				nRest++;
			}
		}


	  	StartTimer = millis();
		insertionSort(restaurants,nRest);
	  	EndTimer = millis();
	  	total = EndTimer - StartTimer;

	  	Serial.print("Insertion sort running time: ");
	  	Serial.print(total);
	  	Serial.print(" ms");
	  	Serial.println();

	}
	
	return nRest;
}

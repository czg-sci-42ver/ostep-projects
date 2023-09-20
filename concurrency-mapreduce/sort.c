/*
https://www.geeksforgeeks.org/quick-sort/
*/
// C code to implement quicksort
 
#include <stdio.h>
#include "sort.h"
 
// Function to swap two elements
void swap(File_info* a, File_info* b)
{
    File_info t = *a;
    *a = *b;
    *b = t;
}
 
// Partition the array using the last element as the pivot
int partition(File_info arr[], int low, int high)
{
    // Choosing the pivot
    int pivot = arr[high].size;
 
    // Index of smaller element and indicates
    // the right position of pivot found so far
    int i = (low - 1);
 
    for (int j = low; j <= high - 1; j++) {
 
        // If current element is smaller than the pivot
        if (arr[j].size < pivot) {
 
            // Increment index of smaller element
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return (i + 1);
}
 
// The main function that implements QuickSort
// arr[] --> Array to be sorted,
// low --> Starting index,
// high --> Ending index
void quickSort(File_info arr[], int low, int high)
{
    if (low < high) {
 
        // pi is partitioning index, arr[p]
        // is now at right place
        int pi = partition(arr, low, high);
 
        // Separately sort elements before
        // partition and after partition
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}
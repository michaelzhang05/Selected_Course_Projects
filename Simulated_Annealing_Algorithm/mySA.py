# -*- coding: utf-8 -*-
"""
Created on Wed Sep 28 12:08:15 2022

Simulated Annealing Alogrithm

@author: Yuxuan Zhang
"""

import random
import math
from TSP import *

def sim_anneal(TSP_problem, no_of_steps, init_temperature):
    # Initial tour
    new_tour = TSP_Problem.generate_random_tour(TSP_problem)
    print("The initial tour is:", new_tour)
    energy = TSP_Problem.evaluate_tour(TSP_problem, new_tour)
    print("The initial tour has energy:", energy)
    T = init_temperature * (no_of_steps - 0) / no_of_steps
    print("The initial temperature is:", T)
    # Some variables
    cnt_tour_tried = 0              # the counter for tours tried
    cnt_tour_accepted = 0           # the counter for tours accepted
    i = 0
    # Mutation Progress
    while(i < no_of_steps):
        # Buff the previous energy and tour
        pre_energy = TSP_Problem.evaluate_tour(TSP_problem, new_tour)
        pre_tour = new_tour
        # Mutate
        new_tour = TSP_Problem.permute_tour(TSP_problem, new_tour)
        cnt_tour_tried = cnt_tour_tried + 1     # a new tour tried
        # Calculate diff energy
        new_energy = TSP_Problem.evaluate_tour(TSP_problem, new_tour)
        diff_energy = pre_energy - new_energy
        # Calculate x
        x = random.random()  # the random number from interval [0,1]
        #print("Parameter x is:", x)
        # Calculate p
        p = math.exp(diff_energy/T)
        # Compare p with x
        if(x <= p):
            cnt_tour_accepted = cnt_tour_accepted + 1       # accept
        else:
            new_tour = pre_tour                             # reject
        # Cooling
        i = i + 1
        T = (init_temperature * (no_of_steps - i) * (no_of_steps - i)) / (no_of_steps * no_of_steps)
        #T = init_temperature * math.pow(0.5, (no_of_steps - i)/no_of_steps)
        #if(cnt_tour_accepted == 20000 or cnt_tour_tried - cnt_tour_accepted ==20000):
        #    break
    # Print Results
    print("Number of tour tried:", cnt_tour_tried)
    print("Number of tours accepted:", cnt_tour_accepted)
    print("The best tour found is:", new_tour)
    print("Best tour has energy:", TSP_Problem.evaluate_tour(TSP_problem, new_tour))

problem = TSP_Problem(Standard_Cities)
sim_anneal(problem, 1000000, 10)

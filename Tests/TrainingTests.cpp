/*
    Copyright (c) 2016 Peter Rudenko

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the Software
    is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
    OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "ThirdParty/Catch/include/catch.hpp"
#include "Helpers.h"
#include "Network.h"
#include <float.h>

using namespace TinyRNN;

static const Value kTrainingRate = 0.25f;

SCENARIO("A perceptron can be trained with a xor function", "[training]")
{
    GIVEN("A single-layer perceptron")
    {
        const int numIterations = RANDOM(2500, 3000);
        const auto networkName = RANDOMNAME();
        const auto contextName = RANDOMNAME();
        
        Layer::Ptr inputLayer(new Layer(2));
        Layer::Ptr hiddenLayer(new Layer(20));
        Layer::Ptr outputLayer(new Layer(1));
        
        REQUIRE(inputLayer->getUuid() != hiddenLayer->getUuid());
        REQUIRE(hiddenLayer->getUuid() != outputLayer->getUuid());
        REQUIRE(outputLayer->getUuid() != inputLayer->getUuid());
        
        inputLayer->connectAllToAll(hiddenLayer);
        hiddenLayer->connectAllToAll(outputLayer);
        
        Network::Ptr network(new Network(networkName, inputLayer, {hiddenLayer}, outputLayer));
        REQUIRE(network->getName() == networkName);
        
        WHEN("The network is trained with some random number of iterations")
        {
            {
                const ScopedTimer timer("Training usual network");
                
                for (int i = 0; i < numIterations; ++i)
                {
                    network->feed({0.0, 1.0});
                    network->train(kTrainingRate, {1.0});
                    
                    network->feed({0.0, 0.0});
                    network->train(kTrainingRate, {0.0});
                    
                    network->feed({1.0, 0.0});
                    network->train(kTrainingRate, {1.0});
                    
                    network->feed({1.0, 1.0});
                    network->train(kTrainingRate, {0.0});
                }
            }
            
            THEN("It gives a reasonable output")
            {
                const auto result1 = network->feed({0.0, 1.0});
                REQUIRE(result1.size() == 1);
                INFO(result1.front());
                REQUIRE(result1.front() > 0.9);
                
                const auto result2 = network->feed({1.0, 0.0});
                REQUIRE(result2.size() == 1);
                INFO(result2.front());
                REQUIRE(result2.front() > 0.9);
                
                const auto result3 = network->feed({0.0, 0.0});
                REQUIRE(result3.size() == 1);
                INFO(result3.front());
                REQUIRE(result3.front() < 0.1);
                
                const auto result4 = network->feed({1.0, 1.0});
                REQUIRE(result4.size() == 1);
                INFO(result4.front());
                REQUIRE(result4.front() < 0.1);
            }
        }
    }
}

static Value crossEntropyErrorCost(const Neuron::Values &targets, const Neuron::Values &outputs)
{
    Value cost = 0.0;
    
    for (size_t i = 0; i < outputs.size(); ++i)
    {
        cost -= ((targets[i] * log(outputs[i] + DBL_MIN)) +
                 ((1 - targets[i]) * log(DBL_MIN - outputs[i])));
    }
    
    return cost;
}

static Value meanSquaredErrorCost(const Neuron::Values &targets, const Neuron::Values &outputs)
{
    Value cost = 0.0;
    
    for (size_t i = 0; i < outputs.size(); ++i)
    {
        cost += (pow(targets[i] - outputs[i], 2.0));
    }
    
    return cost / outputs.size();
}

static Value f(Value x, Value seed)
{
    return seed * 2.0 + cos(x) * seed * 3.0 + tanh(x) * sin(x) * sin(x) * seed * -0.5;
}

SCENARIO("A dbn can be trained to model a random periodic function", "[training]")
{
    GIVEN("A deep belief network")
    {
        const int fxSeed = RANDOM(-1.0, 1.0);
        const int numIterations = RANDOM(2000, 3000);
        Network::Ptr network = Network::Prefabs::feedForward(RANDOMNAME(), 1, { 32, 16, 8, 4, 2 }, 1);
        
        WHEN("The network is trained with some random number of iterations")
        {
            for (int i = 0; i < numIterations; ++i)
            {
                const Value x = RANDOM(-10.0, 10.0);
                network->feed({x});
                network->train(kTrainingRate, {f(x, fxSeed)});
            }
            
            THEN("It gives a reasonable output")
            {
                const int numChecks = RANDOM(50, 100);
                
                for (int i = 0; i < numChecks; ++i)
                {
                    const Value x = RANDOM(-10.0, 10.0);
                    const auto result = network->feed({x});
                    const Value error = meanSquaredErrorCost({f(x, fxSeed)}, result);
                    REQUIRE(error < 0.1);
                }
            }
        }
    }
    
    GIVEN("An unrolled deep belief network")
    {
        const int fxSeed = RANDOM(-1.0, 1.0);
        const int numIterations = RANDOM(2000, 3000);
        Network::Ptr network = Network::Prefabs::feedForward(RANDOMNAME(), 1, { 32, 16, 8, 4, 2 }, 1);
        UnrolledNetwork::Ptr vmNetwork = network->toVM();
        
        WHEN("The network is trained with some random number of iterations")
        {
            for (int i = 0; i < numIterations; ++i)
            {
                const Value x = RANDOM(-10.0, 10.0);
                vmNetwork->feed({x});
                vmNetwork->train(kTrainingRate, {f(x, fxSeed)});
            }
            
            THEN("It gives a reasonable output")
            {
                const int numChecks = RANDOM(50, 100);
                
                for (int i = 0; i < numChecks; ++i)
                {
                    const Value x = RANDOM(-10.0, 10.0);
                    const auto result = vmNetwork->feed({x});
                    const Value error = meanSquaredErrorCost({f(x, fxSeed)}, result);
                    REQUIRE(error < 0.1);
                }
            }
        }
    }
}

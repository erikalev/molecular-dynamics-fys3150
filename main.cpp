#include <iostream>
#include <cstdlib>
#define ALIGNMALLOC 64
#include "math/random.h"

#include "potentials/lennardjones.h"
#include "integrators/velocityverlet.h"
#include "system.h"
#include "statisticssampler.h"
#include "io.h"
#include "unitconverter.h"
#include "cpelapsedtimer.h"
#include "modifiers/berendsenthermostat.h"

using namespace std;

int main(int args, char *argv[])
{
    unsigned int numTimeSteps = 1e2;
    double dt = UnitConverter::timeFromSI(1e-14); // You should try different values for dt as well.
    int numUnitCells = 8;
    float latticeConstant = 5.26;
    // float latticeConstant = 5.885;
    bool loadState = false;
    bool thermostatEnabled = false;
    float temperature = 150;
    if(args>1) {
        dt = UnitConverter::timeFromSI(atof(argv[1])*1e-15);
        numTimeSteps = atoi(argv[2]);
        numUnitCells = atoi(argv[3]);
        latticeConstant = atof(argv[4]);
        loadState = atoi(argv[5]);
        thermostatEnabled = atoi(argv[6]);
        temperature = atof(argv[7]);
    }

    float rCut = UnitConverter::lengthFromAngstroms(2.5*3.405);

    System system;
    StatisticsSampler statisticsSampler;
    BerendsenThermostat thermostat(UnitConverter::temperatureFromSI(temperature), 0.01);

    system.setCutoffRadius(rCut);
    system.createFCCLattice(numUnitCells, UnitConverter::lengthFromAngstroms(latticeConstant), UnitConverter::temperatureFromSI(temperature));
    system.setPotential(new LennardJones(UnitConverter::lengthFromAngstroms(3.405), 1.0, rCut)); // You must insert correct parameters here
    system.setIntegrator(new VelocityVerlet());

    system.removeMomentum();

    IO *movie = new IO(); // To write the state to file
    movie->open("movie.xyz");

    CPElapsedTimer::timeEvolution().start();
    cout << "Will run " << numTimeSteps << " timesteps." << endl;
    for(int timestep=0; timestep<numTimeSteps; timestep++) {
        bool shouldSample = !(timestep % 10) || thermostatEnabled;
        system.setShouldSample(shouldSample);
        system.step(dt);

        if(shouldSample) {
            CPElapsedTimer::sampling().start();
            statisticsSampler.sample(&system);
            CPElapsedTimer::sampling().stop();
        }

        if(thermostatEnabled) {
            CPElapsedTimer::thermostat().start();
            thermostat.apply(&system, &statisticsSampler);
            CPElapsedTimer::thermostat().stop();
        }

        if( !(timestep % 10)) {
            cout << "Step " << timestep << " t= " << UnitConverter::timeToSI(system.currentTime())*1e12 << " ps   Epot/n = " << statisticsSampler.potentialEnergy()/system.numberOfAtoms << "   Ekin/n = " << statisticsSampler.kineticEnergy()/system.numberOfAtoms << "   Etot/n = " << statisticsSampler.totalEnergy()/system.numberOfAtoms <<  endl;
        }
        // movie->saveState(&system);
    }
    CPElapsedTimer::timeEvolution().stop();


    float calculateForcesFraction = CPElapsedTimer::calculateForces().elapsedTime() / CPElapsedTimer::totalTime();
    float halfKickFraction = CPElapsedTimer::halfKick().elapsedTime() / CPElapsedTimer::totalTime();
    float moveFraction = CPElapsedTimer::move().elapsedTime() / CPElapsedTimer::totalTime();
    float updateCellListFraction = CPElapsedTimer::updateCellList().elapsedTime() / CPElapsedTimer::totalTime();
    float periodicBoundaryConditionsFraction = CPElapsedTimer::periodicBoundaryConditions().elapsedTime() / CPElapsedTimer::totalTime();
    float samplingFraction = CPElapsedTimer::sampling().elapsedTime() / CPElapsedTimer::totalTime();
    float timeEvolutionFraction = CPElapsedTimer::timeEvolution().elapsedTime() / CPElapsedTimer::totalTime();
    float thermostatFraction = CPElapsedTimer::thermostat().elapsedTime() / CPElapsedTimer::totalTime();

    cout << endl << "Program finished after " << CPElapsedTimer::totalTime() << " seconds. Time analysis:" << endl;
    cout << fixed
         << "      Time evolution    : " << CPElapsedTimer::timeEvolution().elapsedTime() << " s ( " << 100*timeEvolutionFraction << "%)" <<  endl
         << "      Force calculation : " << CPElapsedTimer::calculateForces().elapsedTime() << " s ( " << 100*calculateForcesFraction << "%)" <<  endl
         << "      Thermostat        : " << CPElapsedTimer::thermostat().elapsedTime() << " s ( " << 100*thermostatFraction << "%)" <<  endl
         << "      Moving            : " << CPElapsedTimer::move().elapsedTime() << " s ( " << 100*moveFraction << "%)" <<  endl
         << "      Half kick         : " << CPElapsedTimer::halfKick().elapsedTime() << " s ( " << 100*halfKickFraction << "%)" <<  endl
         << "      Update cells      : " << CPElapsedTimer::updateCellList().elapsedTime() << " s ( " << 100*updateCellListFraction << "%)" <<  endl
         << "      Periodic boundary : " << CPElapsedTimer::periodicBoundaryConditions().elapsedTime() << " s ( " << 100*periodicBoundaryConditionsFraction << "%)" <<  endl
         << "      Sampling          : " << CPElapsedTimer::sampling().elapsedTime() << " s ( " << 100*samplingFraction << "%)" <<  endl;
    cout << endl << numTimeSteps / CPElapsedTimer::totalTime() << " timesteps / second. " << endl;
    cout << system.numberOfAtoms*numTimeSteps / (1000*CPElapsedTimer::totalTime()) << "k atom-timesteps / second. " << endl;
    // int pairsPerSecond = system.atoms().numberOfComputedForces / (1000*CPElapsedTimer::totalTime());
    // int bytesPerSecond = pairsPerSecond*6*sizeof(float);
    float totalTimePerDay = dt*numTimeSteps/CPElapsedTimer::totalTime() * 86400;
    float nanoSecondsPerDay = UnitConverter::timeToSI(totalTimePerDay)*1e9;
    cout << "Estimated " << nanoSecondsPerDay << " ns simulated time per day" << endl;
    // cout << pairsPerSecond << " pairs computed per second (" << bytesPerSecond/1000000. << " megabytes / sec)" << endl;

    movie->close();

    return 0;
}

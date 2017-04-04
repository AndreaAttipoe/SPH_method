///**************************************************************************
/// SOURCE: Functions related to MPI.
///**************************************************************************
#include "Main.h"
#include "Interface.h"
#include "Physics.h"
#include "Tools.h"
#include "Structures.h"

// For particle exchange/sharing
enum overlap {leftOverlap, noOverlap, rightOverlap, NB_OVERLAP_VALUE};
enum migrate {noMigrate, leftMigrate, rightMigrate, NB_MIGRATE_VALUE};


/*
Input:
    - globalField: filled only on process #0, to be scattered
    - localField: partial field specific to each process. Will contain
    both the domain to be solved and the halos. Need to be done in two
    steps because MPI_Scatterv does not allow overlaps.
    - parameter: to get kh and integrationMethod
Ouput:
    - errorFlag: tells if the number of processor was acceptable or not
*/
void scatterField(Field* globalField, Field* localField, Parameter* parameter){
    // Basic MPI process information
    int nTasks, procID;
	MPI_Comm_size(MPI_COMM_WORLD, &nTasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &procID);
    // Box size (bigger if RK2 to avoid sorting twice at each time step)
    double multiplicator = (parameter->integrationMethod)? 1.1 : 1.0;
    double boxSize = parameter->kh * multiplicator;
    // Checks if the number of processor appropriate (only node 0 has the info)
    int nTotalBoxesX;
    if(procID == 0){
        for(int i=0 ; i<3 ; i++){
            localField->l[i] = globalField->l[i]; // x component will be changed
            localField->u[i] = globalField->u[i]; // x component will be changed
        }
        nTotalBoxesX = ceil((globalField->u[0] - globalField->l[0])/boxSize);
        if(nTotalBoxesX < 3*nTasks){
            std::cout << "Too much processors for the domain" << std::endl;
            std::cout << "-> not yet handled !!!" << std::endl;
        }
        else{std::cout << "Appropriate number of processors" << std::endl;}
    }
    // Broadcasts the total number of boxes along x
    MPI_Bcast(&nTotalBoxesX, 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> startBoxX(nTasks+1); // The last element helps the last process
    for(int i=0 ; i <= nTasks ; i++){
        startBoxX[i] = (nTotalBoxesX*i) / nTasks;
    }

    // Broadcasts the global l and u and determines the correct l[0] and u[0]
    MPI_Bcast(localField->l, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(localField->u, 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    double globall0 = localField->l[0];
    localField->l[0] = globall0 + startBoxX[procID]*boxSize;
    localField->u[0] = globall0 + startBoxX[procID+1]*boxSize;

    std::cout << procID << ": " <<localField->l[0] << " " << localField->u[0] << std::endl;

    // Computes indices and sorts particles
    std::vector<int> nPartNode(nTasks, 0);
    std::vector< std::pair<int,int> > domainIndex(globalField->nTotal);
    std::vector<double> limits(nTasks); // left boundary of each domain along x
    std::vector<int> offset(nTasks);

    if(procID==0){
        for(int i=0 ; i<nTasks ; i++)
            limits[i] = globall0 + startBoxX[i]*boxSize;
        computeDomainIndex(globalField->pos[0], limits, nPartNode, domainIndex, nTasks);
        sortParticles(*globalField, domainIndex); // BE SURE NOT COPY IS PERFOMED !!!!!!!!!!
        // Offset vector
        offset[0] = 0;
        for(int i=1 ; i<nTasks ; i++){
            offset[i] = offset[i-1]+nPartNode[i-1];
        }
        /* DISPLAY
        for(int i=0; i<domainIndex.size(); ++i){
            std::cout<<domainIndex[i].first <<" "<< domainIndex[i].second <<std::endl;
        }
        for(int i=0 ; i<nTasks ; i++)
            std::cout << nPartNode[i] << " ";
        std::cout << std::endl;
        */
    }


    // Shares the number of particle per domain and prepares the vector size
    MPI_Scatter(&nPartNode[0], 1, MPI_INT, &localField->nTotal, 1, MPI_INT, 0, MPI_COMM_WORLD);
    for(int i=0 ; i<3 ; i++){
        localField->pos[i].reserve(localField->nTotal);
        localField->speed[i].reserve(localField->nTotal);
    }
    localField->density.reserve(localField->nTotal);
    localField->pressure.reserve(localField->nTotal);
    localField->mass.reserve(localField->nTotal);
    localField->type.reserve(localField->nTotal);

    std::cout << procID << ": "<<localField->nTotal << std::endl;

    // Scatters globalField into localFields
    for(int i=0 ; i<3 ; i++){
        MPI_Scatterv(&globalField->pos[i][0], &nPartNode[0], &offset[0], MPI_DOUBLE,
                &localField->pos[i][0], localField->nTotal, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Scatterv(&globalField->speed[i][0], &nPartNode[0], &offset[0], MPI_DOUBLE,
                &localField->speed[i][0], localField->nTotal, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    MPI_Scatterv(&(globalField->density[0]), &nPartNode[0], &offset[0], MPI_DOUBLE,
            &(localField->density[0]), localField->nTotal, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Scatterv(&globalField->pressure[0], &nPartNode[0], &offset[0], MPI_DOUBLE,
            &localField->pressure[0], localField->nTotal, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Scatterv(&globalField->mass[0], &nPartNode[0], &offset[0], MPI_DOUBLE,
            &localField->mass[0], localField->nTotal, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Scatterv(&globalField->type[0], &nPartNode[0], &offset[0], MPI_INT,
            &localField->type[0], localField->nTotal, MPI_INT, 0, MPI_COMM_WORLD);

    // Computes nFree, nMoving and nFixed
    localField->nFree = 0;
    localField->nFixed = 0;
    localField->nMoving = 0;
    for(int i=0 ; i<localField->pos[0].size() ; i++){
        switch(localField->type[i]){
            case freePart:
            localField->nFree++;
            case fixedPart:
            localField->nFixed++;
            default:
            localField->nMoving++;
        }
    }

    std::cout << localField->pos[0].size() << std::endl;

    std::cout << "Processor " << procID << " has " << localField->nFree << " free particles, " << localField->nFixed << " fixed particles and " << localField->nMoving << " moving particles." << std::endl;

    // Share edges to halos and determine starting/ending boxes



}

void gatherField(Field* globalField, Field* localField){
    // Gather all the current fields into the global Field (for output file writing for example)

    globalField->nFree = localField->nFree;
    globalField->nFixed = localField->nFixed;
    globalField->nMoving = localField->nMoving;
    globalField->nTotal = localField->nTotal;
    for (int i = 0; i < 3; i++){
      globalField->l[i] = localField->l[i];
      globalField->u[i] = localField->u[i];
      globalField->pos[i] = localField->pos[i];
      globalField->speed[i] = localField->speed[i];
    }
    globalField->mass = localField->mass;
    globalField->type = localField->type;
    globalField->density = localField->density;
    globalField->pressure = localField->pressure;


}

void processUpdate(Field* localField){
    // Sorts the particles
    // Sends the particles that leave the domain
    // Receives the particles that enter the domain
    // Sorts the particles
    // Sends the edges to halos of surrounding domains
    // Receives the halos from edges of surrounding domains

}

void timeStepFinding(Field* currentField){
    // Find on each process the maximum acceptable time step

    // Reduce this information among all processes
}


void computeDomainIndex(std::vector<double> &posX,
    std::vector<double> &limits, std::vector<int> &nbPartNode,
    std::vector< std::pair<int,int> > &index, int nTasks){
    // Loop over particles
    for(int i=0 ; i<posX.size() ; i++){
        int domainNumber = getDomainNumber(posX[i], limits, nTasks);
        index.push_back(std::make_pair(domainNumber,i));
        nbPartNode[domainNumber]++;
    }
}

int getDomainNumber(double x, std::vector<double> &limits, int nTasks){
    // NAIVE !!! (implement a tree based search instead)
    int i;
    for(i=0 ; i<nTasks && x>limits[i] ; i++);
    return i-1;
}


// ------------------------------------------------------------
// FROM SORT.CPP

int computeMigrateIndex(std::vector<double>& posX,
    std::vector< std::pair<int,int> >& index){
    int nResize=0;
    for(unsigned int i=0; i<posX.size(); ++i){
        if(posX[i]>0.8){ // Just in order to test, has to be adapted to the geometry
            index.push_back( std::make_pair(rightMigrate,i) );
            ++nResize;
        }
        else if(posX[i]<0.2){
            index.push_back( std::make_pair(leftMigrate,i) );
            ++nResize;
        }
        else{
            index.push_back( std::make_pair(noMigrate, i) );
        }
    }
    return nResize;
}

void computeOverlapIndex(std::vector<double>& posX,
    std::vector< std::pair<int,int> >& index){
    for(unsigned int i=0; i<posX.size(); ++i){
        if(posX[i]>0.7 && posX[i]<0.8){ // Just in order to test, has to be adapted to the geometry
            index.push_back( std::make_pair(rightOverlap, i) );
        }
        else if(posX[i]<0.3 && posX[i]>0.2){
            index.push_back( std::make_pair(leftOverlap,i) );
        }
        else{
            index.push_back( std::make_pair(noOverlap, i) );
        }
    }
}

bool sortFunction(const std::pair<int,int>& one, const std::pair<int,int>& two){
    return (one.first < two.first);
}

void sortParticles(Field& field, std::vector< std::pair<int,int> >& index){
    // Sort the index vector
    std::sort(index.begin(), index.end(), sortFunction);
    int N=field.pos[0].size();
    // Temporary vectors for sorting
    std::vector<double> tmp(N);
    std::vector<int> tmpType(N);
    // --- Sorts all data one by one ---
    for(int coord=0 ; coord<3 ; coord++){
        // Position reordering
        for(int i=0; i<N; ++i)
            tmp[i]=field.pos[coord][ index[i].second ];
        (field.pos[coord]).swap(tmp);
        // Speed reordering
        for(int i=0; i<N; ++i)
            tmp[i]=field.speed[coord][ index[i].second ];
        (field.speed[coord]).swap(tmp);
    }
    // Density reordering
    for(int i=0; i<N; ++i)
        tmp[i]=field.density[ index[i].second ];
    (field.density).swap(tmp);
    // Pressure reordering
    for(int i=0; i<N; ++i)
        tmp[i]=field.pressure[ index[i].second ];
    (field.pressure).swap(tmp);
    // Mass reordering
    for(int i=0; i<N; ++i)
        tmp[i]=field.mass[ index[i].second ];
    (field.mass).swap(tmp);
    // Type reordering
    for(int i=0; i<N; ++i)
        tmpType[i]=field.type[ index[i].second ];
    (field.type).swap(tmpType);

    //tmp.clear()// Ok?
    //tmp.shrink_to_fit();// Ok?
}

void resizeField(Field& field, int nResize){
    int finalSize=(field.pos[0]).size()-nResize;
    // Position resize
    (field.pos[0]).resize(finalSize);
    (field.pos[0]).shrink_to_fit();
    // Speed resize
    (field.speed[0]).resize(finalSize);
    (field.speed[0]).shrink_to_fit();
}

void displayTest(Field& field, std::vector< std::pair<int,int> >& indexOverlap,
    std::vector< std::pair<int,int> >& indexMigrate){ // DISPLAYER
    std::cout<<"Positon:"<<std::endl;
    for(unsigned int i=0; i<field.pos[0].size(); ++i){
        std::cout<<field.pos[0][i]<<std::endl;
    }

    std::cout<<"Migrate indexes :"<<std::endl;
    for(unsigned int i=0; i<indexMigrate.size(); ++i){
        std::cout<<indexMigrate[i].first <<" "<< indexMigrate[i].second <<std::endl;
    }

    std::cout<<"Overlap indexes :"<<std::endl;
    for(unsigned int i=0; i<indexOverlap.size(); ++i){
        std::cout<<indexOverlap[i].first <<" "<< indexOverlap[i].second <<std::endl;
    }

    std::cout<<"Speed:"<<std::endl;
    for(unsigned int i=0; i<field.speed[0].size(); ++i){
        std::cout<<field.speed[0][i]<<std::endl;
    }
    std::cout<<"END\n"<<std::endl;
}

///**************************************************************************
/// SOURCE: Functions to read the parameters and geometry given by the user.
///**************************************************************************
#include "Physics.h"
#include "Main.h"
#include "Interface.h"

#include <sstream>
#include <algorithm>

#define N_UL 3
#define N_DATA 9
#define N_PARAM 24

enum geomType{cube,cylinder,sphere};
enum boundCondition{freePart, movingPart, fixedPart};

/*
*Input:
*- type: Brick type to be read
*- inFile: Pointer to the input stream associated to the geometry file
*- currentField: Pointer to the structure to fill
*- posFree/posFree/posMoving: vector to store the position of the particles generated during brick reading
*- volVectorFree/Fixed/Moving: vector to store the volume of the particles generated during brick reading
*Decscription:
*Read a brick from the geometry file and generate the position and the volume of the particle inside the brick and store these information in the corresponding vectors.
*/

// Why "int type" and not "GoeomType type" ? would help identify the use of this argument ?
// Why passing vector with a pointer ? Isn't it possible with a reference ?
// Field* currentField useless here ?

void readBrick(int type, std::ifstream* inFile, Field* currentField, std::vector<double>* posFree,
        std::vector<double>* posMoving, std::vector<double>* posFixed,
        std::vector<double>* volVectorFree,  std::vector<double>* volVectorFixed, std::vector<double>* volVectorMoving)
{
    std::string buf;
    int cnt=0;
    char valueArray[1024];
    float brickData[N_DATA];

    while(cnt!=N_DATA)
    {
        std::getline(*inFile, buf);
        if(1==sscanf(buf.c_str(),"%*[^=]=%s", valueArray))
        {
            brickData[cnt]=atof(valueArray);
            ++cnt;
        }
        else{continue;}
    }
    int c=(int)brickData[0];
    float s=brickData[1];
    float r=brickData[2];
    double o[3] = {brickData[3],brickData[4],brickData[5]};
    double L[3] = {brickData[6],brickData[7],brickData[8]};
    int nPart;
    double volPart;
    switch(c)
    {
        case freePart :
        switch(type)
        {
            case cube :
            meshcube(o, L, s, *posFree, &nPart, &volPart, r, true);
            break;
            case cylinder :
            meshcylinder(o, L, s, *posFree, &nPart, &volPart, r, true);
            break;
            case sphere :
            meshsphere(o, L, s, *posFree, &nPart, &volPart, r, true);
            break;
        }
        for(cnt=0; cnt<nPart; ++cnt)
        {
            (*volVectorFree).push_back(volPart);
        }
        break;
        case fixedPart :
        switch(type)
        {
            case cube :
            meshcube(o, L, s, *posFixed, &nPart, &volPart, r, true);
            break;
            case cylinder :
            meshcylinder(o, L, s, *posFixed, &nPart, &volPart, r, true);
            break;
            case sphere :
            meshsphere(o, L, s, *posFixed, &nPart, &volPart, r, true);
            break;
        }
        for(cnt=0; cnt<nPart; ++cnt)
        {
            (*volVectorFixed).push_back(volPart);
        }
        break;
        case movingPart :
        switch(type)
        {
            case cube :
            meshcube(o, L, s, *posMoving, &nPart, &volPart, r, true);
            break;
            case cylinder :
            meshcylinder(o, L, s, *posMoving, &nPart, &volPart, r, true);
            break;
            case sphere :
            meshsphere(o, L, s, *posMoving, &nPart, &volPart, r, true);
            break;
        }
        for(cnt=0; cnt<nPart; ++cnt)
        {
            (*volVectorMoving).push_back(volPart);
        }
        break;
    }
}

/*
*Input:
*- filename: name of the geometry file to read
*- volVector: vector to store the volume of the particles generated during reading the whole geometry
*Output:
*- 1: error
*- 0: no error
*Decscription:
*Read a entire geometry file and generate the position and the volume of the particles and store these informations in a structure.
*/
int readGeometry(std::string filename, Field* currentField, std::vector<double>* volVector)
{
    std::vector<double> posFree, posFixed, posMoving, volVectorFree, volVectorFixed, volVectorMoving;
    std::ifstream inFile(filename);
    std::string buf;
    char valueArray[1024];
    int cnt=0;
    std::getline(inFile, buf);
    if(buf!="#GEOM1")
    {
        std::cout << "Wrong Geometry file type.\n" << std::endl;
        return 1;
    }
    while(true) // UNTIL #END_G
    {
        std::getline(inFile, buf);
        buf.erase(std::remove(buf.begin(), buf.end(), ' '),buf.end());
        switch(buf[0])
        {
            case '%' :
            continue;
            break;
            case '#' :
            buf.erase (0,1);
            if(buf=="domsz")
            {
                while(cnt!=N_UL)
                {
                    std::getline(inFile, buf);
                    if(1==sscanf(buf.c_str(),"%*[^=]=%s", valueArray))
                    {
                        currentField->u[cnt]=atof(valueArray);
                        ++cnt;
                    }
                    else
                    continue;
                }
                cnt = 0;
                while(cnt!=N_UL)
                {
                    std::getline(inFile, buf);
                    if(1==sscanf(buf.c_str(),"%*[^=]=%s", valueArray))
                    {
                        currentField->l[cnt]=atof(valueArray);
                        ++cnt;
                    }
                    else
                    continue;
                }
            }
            else if(buf=="brick")
            {
                readBrick(cube,&inFile, currentField,
                    &posFree, &posMoving, &posFixed,
                    &volVectorFree, &volVectorFixed, &volVectorMoving);
            }
            else if(buf=="cylin")
            {
                readBrick(cylinder,&inFile, currentField,
                    &posFree, &posMoving, &posFixed,
                    &volVectorFree, &volVectorFixed, &volVectorMoving);
            }
            else if(buf=="spher")
            {
                readBrick(sphere,&inFile, currentField,
                    &posFree, &posMoving, &posFixed,
                    &volVectorFree, &volVectorFixed, &volVectorMoving);
            }
            else if(buf=="END_G")
            {
                // Number of particles
                currentField->nFree=posFree.size()/3;
                currentField->nFixed=posFixed.size()/3;
                currentField->nMoving=posMoving.size()/3;
                currentField->nTotal= currentField->nFree + currentField->nFixed + currentField->nMoving;
                // Position and volume vector sorting
                posFree.insert(posFree.end(), posFixed.begin(), posFixed.end());
                posFree.insert(posFree.end(), posMoving.begin(), posMoving.end());
                volVectorFree.insert(volVectorFree.end(), volVectorFixed.begin(), volVectorFixed.end());
                volVectorFree.insert(volVectorFree.end(), volVectorMoving.begin(), volVectorMoving.end());
                (*volVector)=volVectorFree;
                currentField->pos=posFree;
                return 0;
            }
            else
            {
                std::cout <<"Unknown '"<<buf<<"' identifier.\n" << std::endl;
                return 1;
            }
            break;
            default :
            continue;
        }
    }
}

/*
*Input:
*- filename: name of the parameter file to read
*- parameter: pointer the the structure to fill
*Output:
*- 1: error
*- 0: no error
*Decscription:
*Read a parameter file and the information in a structure.
*/
int readParameter(std::string filename, Parameter* parameter)
{
    // open a file geometry.kzr
    std::ifstream inFile(filename);
    std::string buf;
    std::getline(inFile, buf);
    if(buf!="#PARA1")
    {
        std::cout << "Wrong parameter file type.\n" << std::endl;
        return 1;
    }
    while(true)
    {
        std::getline(inFile, buf);
        buf.erase(std::remove(buf.begin(), buf.end(), ' '),buf.end());
        switch(buf[0])
        {
            case '%' :
            continue;
            break;
            case '#' :
            buf.erase (0,1);
            if(buf=="param")
            {
                int cnt=0;
                char valueArray[1024];
                while(cnt!=N_PARAM)
                {
                    std::getline(inFile, buf);
                    if(1==sscanf(buf.c_str(),"%*[^=]=%s", valueArray))
                    {
                        if(cnt==0)
                        parameter->kh=atof(valueArray);
                        if(cnt==1)
                        parameter->k=atof(valueArray);
                        if(cnt==2)
                        parameter->T=atof(valueArray);
                        if(cnt==3)
                        parameter->densityRef=atof(valueArray);
                        if(cnt==4)
                        parameter->B=atof(valueArray);
                        if(cnt==5)
                        parameter->gamma=atof(valueArray);
                        if(cnt==6)
                        parameter->g=atof(valueArray);
                        if(cnt==7)
                        parameter->writeInterval=atof(valueArray);
                        if(cnt==8)
                        parameter->charactTime=atof(valueArray);
                        if(cnt==9)
                        parameter->c=atof(valueArray);
                        if(cnt==10)
                        parameter->alpha=atof(valueArray);
                        if(cnt==11)
                        parameter->beta=atof(valueArray);
                        if(cnt==12)
                        parameter->epsilon=atof(valueArray);
                        if(cnt==13)
                        parameter->movingDirection[0]=atof(valueArray);
                        if(cnt==14)
                        parameter->movingDirection[1]=atof(valueArray);
                        if(cnt==15)
                        parameter->movingDirection[2]=atof(valueArray);
                        if(cnt==16)
                        parameter->kernel=(Kernel) atoi(valueArray);
                        if(cnt==17)
                        parameter->viscosityModel=(ViscosityModel) atoi(valueArray);
                        if(cnt==18)
                        parameter->integrationMethod=(IntegrationMethod) atoi(valueArray);
                        if(cnt==19)
                        parameter->densityInitMethod=(DensityInitMethod) atoi(valueArray);
                        if(cnt==20)
                        parameter->stateEquationMethod=(StateEquationMethod) atoi(valueArray);
                        if(cnt==21)
                        parameter->massInitMethod=(MassInitMethod) atoi(valueArray);
                        if(cnt==22)
                        parameter->speedLaw=(SpeedLaw) atoi(valueArray);
                        if(cnt==23)
                        parameter->format = (Format) atoi(valueArray);
                        ++cnt;
                    }
                    else{continue;}
                }
            }
            else if(buf=="END_F")
            {
                return 0;
            }
            else
            {
                std::cout <<"Unknown '"<<buf<<"' identifier. \n" << std::endl;
                return 1;
            }
            break;
            default :
            continue;
        }
    }
}

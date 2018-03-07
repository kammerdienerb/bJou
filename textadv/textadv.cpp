#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <time.h>
#include <array>
#include <string>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
std::string userName;

std::string directory;
char* path;
char size[500];
std::string s_savePath;
char* dirname="Pixeless Saves";



#ifdef _WIN32				//OS????
#include <direct.h>
#include <Windows.h>
#include <Lmcons.h>
#define _POSIX_SOURCE
#define OS
std::string OperatingSystem="WINDOWS";
#define getUser
void func()
{
    char acUserName[100];
    DWORD nUserName = sizeof(acUserName);
    if (GetUserName(acUserName, &nUserName))
        userName = acUserName;
}

#define createDirectory
void makeDirectory()
{
    s_savePath=directory+dirname;
    const char *c_savePath = s_savePath.c_str();
	mkdir(c_savePath);
}

#endif

#ifdef __APPLE__

#include <unistd.h>                 //include for getcwd
#define OS
std::string OperatingSystem="OSX";
#define getUser
    void func()
    {
    
    }

#define createDirectory
void makeDirectory()
{
    s_savePath=directory+dirname;
    const char *c_savePath = s_savePath.c_str();
	mkdir(c_savePath, S_IRWXU);
}
#endif




//*!*!*!*!*!*!*!---Variables and Strings---*!*!*!*!*!*!*!//

//Player Variables
bool newGame=true;
bool superUser=false;
bool saveExists=false;
std::string saveFile="save";
std::string name;
int gld=31;
int keys=0;
float combatBoost=.1;
float item1Boost=0;
float item2Boost=0;
float item3Boost=0;
float item4Boost=0;
float item5Boost=0;
float playerSkill=(combatBoost+item1Boost+item2Boost+item3Boost+item4Boost+item5Boost);

//Command Strings
std::string userInput;
//
std::string yes="yes";
std::string no="no";
//
std::string n="n";
std::string north="north";
//
std::string s="s";
std::string south="south";
//
std::string e="e";
std::string east="east";
//
std::string w="w";
std::string west="west";
//
std::string h="h";
std::string help="help";
//
std::string gold="gold";
std::string items="items";
std::string drop="drop";
std::string skill="skill";
//
std::string wumbo="wumbo";
//
std::string save="save";
std::string savePath="/Users/donnykammerdiener/Documents/Pixeless/save.txt";
//
std::string quit="quit";
//
std::string sudo="sudo";
std::string travel="travel";
std::string addgold="addgold";
std::string addkey="addkey";
std::string additems="additem";
std::string setskill="setskill";



//Location Variables
int x=9;
int y=27;
bool validLocation=true;

//Item Variables
std::string empty="(empty)";
std::string item1=empty;
std::string item2=empty;
std::string item3=empty;
std::string item4=empty;
std::string item5=empty;
std::string tempItem;
std::string trout="Trout";
std::string sword2="Sword--(SkillLevel+2)";
std::string bow2="Bow--(SkillLevel+2)";
std::string staff2="Staff--(SkillLevel+2)";
std::string flamingHelm="Flaming_Helm--(SkillLevel+7)";
std::string wumboWand="Wumbo_Wand--(SkillLevel+15)";
std::string guardshield="Guard's_Shield--(SkillLevel+2)";
bool wumboWandTaken=false;
int troutbuy=5;
int troutsell=2;
int sword2buy=30;
int sword2sell=9;
int bow2buy=30;
int bow2sell=9;
int staff2buy=30;
int staff2sell=9;
int flaminghelmbuy=900;
int flaminghelmsell=350;

//Combat Variables
bool ratWin=false;
bool wolfWin=false;
bool greyDragonWin=false;


int ratWinProb=96;
float ratBoost=.08;

int wolfWinProb=79;
float wolfBoost=.53;

int greyDragWinProb=33;
int greyDragBoost=14;

//Quest Variables
bool QuestStartl12x30=false;
int QuestCompletel12x30=0; //QuestComplete vars - 0 = requirments not met, 1 = requirments met but not verified, 2 = met and verified, >2 = null for non-repetition
int QuestCounterl12x30=0;


//*!*!*!*!*!*!*!---Functions---*!*!*!*!*!*!*!//
//Refresh Skill Level Value

int updateSkill()
{
    playerSkill=(combatBoost+item1Boost+item2Boost+item3Boost+item4Boost+item5Boost);
    
    return 0;
}



//Location Register 2D Array
int registerArray [3][1000]=
{
    {0}
};



int row;
int check;
bool locationExists=false;


int checkForLocationRegistry()
{
    int row=0;
    locationExists=false;
    
    while ((row<1000)&&(registerArray[0][row])!=x)
    {
        row++;
    }
    
    while (row<1000)
    {
        if ((registerArray[1][row])==y)
        {
            locationExists=true;
            goto End;
        }
        
        else
        {
            row++;
        }
    }
    
End:
    return 0;
}


int locRegister()
{
    checkForLocationRegistry();
    row=0;
    
    if (locationExists==false)
    {
        while ((registerArray[0][row])!=0)
        {
            row++;
        }
        
        (registerArray [0][row])=x;
        (registerArray [1][row])=y;
    }
    
    return 0;
}


int visitedRegister()
{
    row=0;
    
    while ((row<1000)&&((registerArray[0][row])!=x)&&((registerArray[0][row])!=y))
    {
        row++;
    }
    
    ((registerArray [2][row])=1);
    
    return 0;
}


bool checkRegister()
{
    while ((row<1000)&&((registerArray[0][row])!=x)&&((registerArray[0][row])!=y))
    {
        row++;
    }
    
    if ((registerArray[2][row])==0)
    {
        return false;
    }
    
    if ((registerArray[2][row])==1)
    {
        return true;
    }
}



//Save and Load

getUser;

void getPath()
{
	func();
    
    path=getcwd(size, sizeof size);
    directory.assign(path,size);
    
    std::string docs;
    
    if (OperatingSystem=="OSX")
    {
        docs="Documents/";
    }
    
    if (OperatingSystem=="WINDOWS")
    {
        docs="C:\\Users\\"+userName+"\\My Documents\\";
    }
    
    directory=directory+docs;
}

createDirectory;


int saveGame()
{
    if (saveExists==false)
    {
        getPath();
        makeDirectory();
        
        std::cout<<"\nWhat would you like your save file to be called?\n";
        std::cin>>saveFile;
        
        std::string slash;
        if (OperatingSystem=="OSX")
        {
            slash="/";
        }
        else
        {
            slash="\\";
        }
        
        std::ofstream textfile (s_savePath+slash+saveFile+".txt");
        
        
        if (textfile)
        {
            std::cout<<"\nOkay. "<<saveFile<<" created!";
        }
        else
        {
            std::cout<<"Error creating "<<saveFile<<"...";
        }
        
        textfile.close();
    }
    
    saveExists=true;
    int regrow=0;
    int regcol=0;
    
    
    
    std::fstream textfile;
    
    std::string slash;
    if (OperatingSystem=="OSX")
    {
        slash="/";
    }
    else
    {
        slash="\\";
    }
    
    textfile.open(s_savePath+slash+saveFile+".txt");
    
    if (!textfile)
    {
        std::cout<<"Error saving... Make sure that your save file is in the correct folder.\n";
        goto saveEnd;
    }
    
    textfile<<saveExists<<"\n";
    textfile<<name<<"\n";
    textfile<<gld<<"\n";
	textfile<<keys<<"\n";
    textfile<<combatBoost<<"\n";
    textfile<<item1Boost<<"\n";
    textfile<<item2Boost<<"\n";
    textfile<<item3Boost<<"\n";
    textfile<<item4Boost<<"\n";
    textfile<<item5Boost<<"\n";
    textfile<<playerSkill<<"\n";
    textfile<<x<<"\n";
    textfile<<y<<"\n";
    textfile<<item1<<"\n";
    textfile<<item2<<"\n";
    textfile<<item3<<"\n";
    textfile<<item4<<"\n";
    textfile<<item5<<"\n";
	textfile<<QuestStartl12x30<<"\n";
	textfile<<QuestCompletel12x30<<"\n";
	textfile<<QuestCounterl12x30<<"\n";
    textfile<<wumboWandTaken<<"\n";
    
    //Writes registerArray to save.txt up to 1000 rows of 3 columns
    while (regrow<1000)
    {
        while (regcol<3)
        {
            textfile<<registerArray[regcol][regrow];
            regcol++;
            textfile<<"\n";
        }
        
        regrow++;
        regcol=0;
    }
    
    
    textfile.close();
    
    std::cout<<"\nGame saved...\n";
    
saveEnd:
    return 0;
}

int loadGame()
{
    getPath();
    s_savePath=directory+dirname;
    
    std::cout<<"\nWhich save?\n";
    std::cin>>saveFile;
    
    int regrow=0;
    int regcol=0;
    
    std::fstream textfile;
    
    std::string slash;
    if (OperatingSystem=="OSX")
    {
        slash="/";
    }
    else
    {
        slash="\\";
    }
    
	textfile.open(s_savePath+slash+saveFile+".txt");
    
	if (!textfile)
    {
        std::cout<<"Error loading... Make sure that your save file is in the correct folder.\nStarting new game..\n\n";
		goto loadEnd;
    }
    
    if (textfile.is_open())
    {
        textfile>>saveExists;
        textfile>>name;
        textfile>>gld;
		textfile>>keys;
        textfile>>combatBoost;
        textfile>>item1Boost;
        textfile>>item2Boost;
        textfile>>item3Boost;
        textfile>>item4Boost;
        textfile>>item5Boost;
        textfile>>playerSkill;
        textfile>>x;
        textfile>>y;
        textfile>>item1;
        textfile>>item2;
        textfile>>item3;
        textfile>>item4;
        textfile>>item5;
        textfile>>QuestStartl12x30;
		textfile>>QuestCompletel12x30;
		textfile>>QuestCounterl12x30;
		textfile>>wumboWandTaken;
        
        
        while (regrow<1000)
        {
            while (regcol<3)
            {
                textfile>>registerArray[regcol][regrow];
                regcol++;
            }
            
            regrow++;
            regcol=0;
        }
        
        textfile.close();
    }
    
    if (saveExists==false)
    {
        std::cout<<"\nCan't find save data.\nStarting new game...\n\n";
        newGame=true;
    }
    
    else
    {
        newGame=false;
    }
    
loadEnd:
    return 0;
}

int askLoadGame()
{
    std::cout<<"Load game?\n";
    std::cin>>userInput;
    if (userInput==yes)
    {
        loadGame();
    }
    
    if (userInput==no)
    {
        std::cout<<"Okay. New game...\n\n\n";
    }
    
    if ((userInput!=yes)&&(userInput!=no))
    {
        askLoadGame();
    }
    
    return 0;
}

//Check Location Parameters
int checkParams()
{
    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    //
    if ((x==1)&&((y==2)||(y==3)||(y==7)||(y==13)||(y==14)||(y==15)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)))
    {}
    if ((x==1)&&((y==0)||(y==1)||(y==4)||(y==5)||(y==6)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==2)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==13)||(y==14)||(y==15)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)))
    {}
    if ((x==2)&&((y==0)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==3)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==13)||(y==14)||(y==15)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==3)&&((y==0)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==4)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==13)||(y==14)||(y==15)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==4)&&((y==0)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==5)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==5)&&((y==0)||(y==10)||(y==11)||(y==12)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==6)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==6)&&((y==0)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==7)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==7)&&((y==0)||(y==19)||(y==20)||(y==21)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==8)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)))
    {}
    if ((x==8)&&((y==0)||(y==19)||(y==20)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==9)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)))
    {}
    if ((x==9)&&((y==0)||(y==1)||(y==2)||(y==19)||(y==20)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==10)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)))
    {}
    if ((x==10)&&((y==0)||(y==1)||(y==2)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==11)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)))
    {}
    if ((x==11)&&((y==0)||(y==1)||(y==2)||(y==19)||(y==20)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==12)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)))
    {}
    if ((x==12)&&((y==0)||(y==1)||(y==2)||(y==19)||(y==20)||(y==21)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==13)&&((y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)))
    {}
    if ((x==13)&&((y==0)||(y==1)||(y==2)||(y==20)||(y==21)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==14)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==14)&&((y==0)||(y==19)||(y==20)||(y==21)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==15)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==15)&&((y==0)||(y==19)||(y==20)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==16)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==16)&&((y==0)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==17)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==17)&&((y==0)||(y==18)||(y==19)||(y==20)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==18)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)))
    {}
    if ((x==18)&&((y==0)||(y==18)||(y==19)||(y==20)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==19)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)))
    {}
    if ((x==19)&&((y==0)||(y==18)||(y==19)||(y==20)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==20)&&((y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)))
    {}
    if ((x==20)&&((y==0)||(y==18)||(y==19)||(y==20)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==21)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)))
    {}
    if ((x==21)&&((y==0)||(y==1)||(y==2)||(y==18)||(y==19)||(y==20)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==22)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)))
    {}
    if ((x==22)&&((y==0)||(y==1)||(y==2)||(y==18)||(y==19)||(y==20)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==23)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)))
    {}
    if ((x==23)&&((y==0)||(y==1)||(y==2)||(y==19)||(y==20)||(y==21)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==24)&&((y==3)||(y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)))
    {}
    if ((x==24)&&((y==0)||(y==1)||(y==2)||(y==20)||(y==21)||(y==40)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==25)&&((y==4)||(y==5)||(y==6)||(y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==40)))
    {}
    if ((x==25)&&((y==0)||(y==1)||(y==2)||(y==3)||(y==21)||(y==22)||(y==23)||(y==24)||(y==41)))
    {
        validLocation=false;
    }
    //
    if ((x==26)&&((y==7)||(y==8)||(y==9)||(y==10)||(y==11)||(y==12)||(y==13)||(y==14)||(y==15)||(y==16)||(y==17)||(y==18)||(y==19)||(y==20)||(y==40)))
    {}
    if ((x==26)&&((y==0)||(y==1)||(y==2)||(y==3)||(y==4)||(y==5)||(y==6)||(y==21)||(y==22)||(y==23)||(y==24)||(y==25)||(y==26)||(y==27)||(y==28)||(y==29)||(y==30)||(y==31)||(y==32)||(y==33)||(y==34)||(y==35)||(y==36)||(y==37)||(y==38)||(y==39)||(y==41)))
    {
        validLocation=false;
    }
    
    
    //yyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
    
    //
    if ((y==1)&&((x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)))
    {}
    if ((y==1)&&((x==0)||(x==1)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==2)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)))
    {}
    if ((y==2)&&((x==0)||(x==9)||(x==10)||(x==11)||(x==12)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==3)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)))
    {}
    if ((y==3)&&((x==0)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==4)&&((x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==4)&&((x==0)||(x==1)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==5)&&((x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==5)&&((x==0)||(x==1)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==6)&&((x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==6)&&((x==0)||(x==1)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==7)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==7)&&((x==0)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==8)&&((x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==8)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==9)&&((x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==9)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==10)&&((x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==10)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==11)&&((x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==11)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==12)&&((x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==12)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==13)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==13)&&((x==0)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==14)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==14)&&((x==0)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==15)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==15)&&((x==0)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==16)&&((x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==16)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==17)&&((x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==17)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==18)&&((x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==18)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==19)&&((x==10)||(x==13)||(x==16)||(x==24)||(x==25)||(x==26)))
    {}
    if ((y==19)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==11)||(x==12)||(x==14)||(x==15)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==20)&&((x==10)||(x==16)||(x==25)||(x==26)))
    {}
    if ((y==20)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==21)&&((x==8)||(x==9)||(x==10)||(x==11)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)))
    {}
    if ((y==21)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==12)||(x==13)||(x==14)||(x==23)||(x==24)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==22)&&((x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)))
    {}
    if ((y==22)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==23)&&((x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)))
    {}
    if ((y==23)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==24)&&((x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)))
    {}
    if ((y==24)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==25)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==25)&&((x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==25)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==26)&&((x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==26)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==27)&&((x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==27)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==28)&&((x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==28)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==29)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==29)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==30)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==30)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==31)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==31)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==32)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==32)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==33)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==33)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==34)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==25)))
    {}
    if ((y==34)&&((x==0)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==35)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==24)||(x==25)))
    {}
    if ((y==35)&&((x==0)||(x==20)||(x==21)||(x==22)||(x==23)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==36)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==24)||(x==25)))
    {}
    if ((y==36)&&((x==0)||(x==20)||(x==21)||(x==22)||(x==23)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==37)&&((x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==24)||(x==25)))
    {}
    if ((y==37)&&((x==0)||(x==8)||(x==9)||(x==10)||(x==11)||(x==20)||(x==21)||(x==22)||(x==23)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==38)&&((x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==12)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==24)||(x==25)))
    {}
    if ((y==38)&&((x==0)||(x==1)||(x==2)||(x==8)||(x==9)||(x==10)||(x==11)||(x==20)||(x==21)||(x==22)||(x==23)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==39)&&((x==12)||(x==13)||(x==19)||(x==24)||(x==25)))
    {}
    if ((y==39)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==20)||(x==21)||(x==22)||(x==23)||(x==26)||(x==27)))
    {
        validLocation=false;
    }
    //
    if ((y==40)&&((x==12)||(x==25)||(x==26)))
    {}
    if ((y==40)&&((x==0)||(x==1)||(x==2)||(x==3)||(x==4)||(x==5)||(x==6)||(x==7)||(x==8)||(x==9)||(x==10)||(x==11)||(x==13)||(x==14)||(x==15)||(x==16)||(x==17)||(x==18)||(x==19)||(x==20)||(x==21)||(x==22)||(x==23)||(x==24)||(x==27)))
    {
        validLocation=false;
    }
    
    return 0;
}



//Item Array and List
char itemIDArray[100][100]=
{
    /*itemID*/
    /*0*/"Trout",
    /*1*/"Sword--(SkillLevel+2)",
    /*2*/"Bow--(SkillLevel+2)",
    /*3*/"Staff--(SkillLevel+2)",
    /*4*/"Flaming_Helm--(SkillLevel+7)",
    /*5*/"Wumbo_Wand--(SkillLevel+15)",
    /*6*/"Guard's_Shield--(SkillLevel+2)"
    /*7*/
    /*8*/
    /*9*/
    /*10*/
    /*11*/
    /*12*/
    /*13*/
    /*14*/
    /*15*/
    /*16*/
    /*17*/
    /*18*/
    /*19*/
    /*20*/
    /*21*/
    /*22*/
    /*23*/
    /*24*/
    /*25*/
    /*26*/
    /*27*/
    /*28*/
    /*29*/
    /*30*/
    /*31*/
    /*32*/
    /*33*/
    /*34*/
    /*35*/
    /*36*/
    /*37*/
    /*38*/
    /*39*/
    /*40*/
    /*41*/
    /*42*/
    /*43*/
    /*44*/
    /*45*/
    /*46*/
    /*47*/
    /*48*/
    /*49*/
    /*50*/
    /*51*/
    /*52*/
    /*53*/
    /*54*/
    /*55*/
    /*56*/
    /*57*/
    /*58*/
    /*59*/
    /*60*/
    /*61*/
    /*62*/
    /*63*/
    /*64*/
    /*65*/
    /*66*/
    /*67*/
    /*68*/
    /*69*/
    /*70*/
    /*71*/
    /*72*/
    /*73*/
    /*74*/
    /*75*/
    /*76*/
    /*77*/
    /*78*/
    /*79*/
    /*80*/
    /*81*/
    /*82*/
    /*83*/
    /*84*/
    /*85*/
    /*86*/
    /*87*/
    /*88*/
    /*89*/
    /*90*/
    /*91*/
    /*92*/
    /*93*/
    /*94*/
    /*95*/
    /*96*/
    /*97*/
    /*98*/
    /*99*/
};

int tempItemID;

//Item Properties in Array (itemID, purchase value, sale value, skill boost)
int itemPListArray [4][100]=
{
    {0}
};

int populateItemPList()
{
    //ID's
    itemPListArray [0][0]=0;
    itemPListArray [0][1]=1;
    itemPListArray [0][2]=2;
    itemPListArray [0][3]=3;
    itemPListArray [0][4]=4;
    itemPListArray [0][5]=5;
    itemPListArray [0][6]=6;
    itemPListArray [0][7]=7;
    itemPListArray [0][8]=8;
    itemPListArray [0][9]=9;
    //Purchase Values
    itemPListArray [1][0]=5;
    itemPListArray [1][1]=30;
    itemPListArray [1][2]=30;
    itemPListArray [1][3]=30;
    itemPListArray [1][4]=900;
    itemPListArray [1][5]=0;
    itemPListArray [1][6]=0;
    itemPListArray [1][7]=0;
    itemPListArray [1][8]=0;
    itemPListArray [1][9]=0;
    //Sale Value
    itemPListArray [2][0]=2;
    itemPListArray [2][1]=9;
    itemPListArray [2][2]=9;
    itemPListArray [2][3]=9;
    itemPListArray [2][4]=350;
    itemPListArray [2][5]=0;
    itemPListArray [2][6]=50;
    itemPListArray [2][7]=0;
    itemPListArray [2][8]=0;
    itemPListArray [2][9]=0;
    //Skill Boost
    itemPListArray [3][0]=0;
    itemPListArray [3][1]=2;
    itemPListArray [3][2]=2;
    itemPListArray [3][3]=2;
    itemPListArray [3][4]=7;
    itemPListArray [3][5]=15;
    itemPListArray [3][6]=2;
    itemPListArray [3][7]=0;
    itemPListArray [3][8]=0;
    itemPListArray [3][9]=0;
    
    return 0;
}

//Stat-Changing Items
int itemStats()
{
    int item1IDCounter=0;
    int item2IDCounter=0;
    int item3IDCounter=0;
    int item4IDCounter=0;
    int item5IDCounter=0;
    
    while (((itemIDArray[item1IDCounter])!=item1)&&(item1IDCounter<100))
    {
        item1IDCounter++;
    }
    
    while (((itemIDArray[item2IDCounter])!=item2)&&(item2IDCounter<100))
    {
        item2IDCounter++;
    }
    
    while (((itemIDArray[item3IDCounter])!=item3)&&(item3IDCounter<100))
    {
        item3IDCounter++;
    }
    
    while (((itemIDArray[item4IDCounter])!=item4)&&(item4IDCounter<100))
    {
        item4IDCounter++;
    }
    
    while (((itemIDArray[item5IDCounter])!=item5)&&(item5IDCounter<100))
    {
        item5IDCounter++;
    }
    
    item1Boost=itemPListArray[3][item1IDCounter];
    item2Boost=itemPListArray[3][item2IDCounter];
    item3Boost=itemPListArray[3][item3IDCounter];
    item4Boost=itemPListArray[3][item4IDCounter];
    item5Boost=itemPListArray[3][item5IDCounter];
    
    return 0;
}

//Add Item
int addItem()
{
    tempItem=itemIDArray[tempItemID];
    
    
    if (item1==empty)
    {
        item1=tempItem;
        goto addItemEnd;
    }
    if (item2==empty)
    {
        item2=tempItem;
        goto addItemEnd;
    }
    if (item3==empty)
    {
        item3=tempItem;
        goto addItemEnd;
    }
    if (item4==empty)
    {
        item4=tempItem;
        goto addItemEnd;
    }
    if (item5==empty)
    {
        item5=tempItem;
        goto addItemEnd;
    }
    
    if ((item1!=empty)&&(item2!=empty)&&(item3!=empty)&&(item4!=empty)&&(item5!=empty))
    {
    addItemq1:
        std::cout<<"\nInventory is full!\nDrop item?\n";
        std::cin>>userInput;
        if (userInput==yes)
        {
        addItemq2:
            std::cout<<"\nWhich item?\n(Reply: 'item(1-5)' --- ex. item1)\n";
            std::cout<<"Inventory:\n\n1. "<<item1<<"\n2. "<<item2<<"\n3. "<<item3<<"\n4. "<<item4<<"\n5. "<<item5<<"\n";
            std::cin>>userInput;
            if (userInput=="item1")
            {
                std::cout<<"\nDropped "<<item1<<".";
                item1=tempItem;
            }
            if (userInput=="item2")
            {
                std::cout<<"\nDropped "<<item2<<".";
                item2=tempItem;
            }
            if (userInput=="item3")
            {
                std::cout<<"\nDropped "<<item3<<".";
                item3=tempItem;
            }
            if (userInput=="item4")
            {
                std::cout<<"\nDropped "<<item4<<".";
                item4=tempItem;
            }
            if (userInput=="item5")
            {
                std::cout<<"\nDropped "<<item5<<".";
                item5=tempItem;
            }
            if ((userInput!="item1")&&(userInput!="item2")&&(userInput!="item3")&&(userInput!="item4")&&(userInput!="item5"))
            {
                goto addItemq2;
            }
            
            goto addItemEnd;
        }
        if (userInput==no)
        {
        addItemq3:
            std::cout<<"\nAre you sure? You may not be able to come back and get this item.\n";
            std::cin>>userInput;
            if (userInput==yes)
            {
                std::cout<<"Okay.\n\n";
                goto addItemEnd2;
            }
            if (userInput==no)
            {
                goto addItemq1;
            }
            if ((userInput!=yes)&&(userInput!=no))
            {
                goto addItemq3;
            }
            
        }
        if ((userInput!=yes)&&(userInput!=no))
        {
            goto addItemq1;
        }
    }
addItemEnd:
    std::cout<<"\nTook "<<tempItem<<".\n";
addItemEnd2:
    itemStats();
    return 0;
}

//Sell Item
int sellItem()
{
loseItemq1:
    std::cout<<"\nWhich item?\n(Reply: 'item(1-5)' --- ex. item1)\n";
    std::cout<<"Inventory:\n\n1. "<<item1<<"\n2. "<<item2<<"\n3. "<<item3<<"\n4. "<<item4<<"\n5. "<<item5<<"\n";
    
    int item1IDCounter=0;
    int item2IDCounter=0;
    int item3IDCounter=0;
    int item4IDCounter=0;
    int item5IDCounter=0;
    
    std::cin>>userInput;
    
    if ((userInput=="item1")&&(item1!=empty))
    {
        while (((itemIDArray[item1IDCounter])!=item1)&&(item1IDCounter<100))
        {
            item1IDCounter++;
        }
        
        gld=((gld+(itemPListArray[2][item1IDCounter])));
        
        std::cout<<"\nSold "<<item1<<"for "<<itemPListArray[2][item1IDCounter]<<" gold.";
        
        item1=empty;
        
        goto loseItemEnd;
    }
    
    if ((userInput=="item1")&&(item1==empty))
    {
        std::cout<<"\nThere's nothing there to sell!\n";
    }
    
    if ((userInput=="item2")&&(item2!=empty))
    {
        while (((itemIDArray[item2IDCounter])!=item2)&&(item2IDCounter<100))
        {
            item2IDCounter++;
        }
        
        gld=((gld+(itemPListArray[2][item2IDCounter])));
        
        std::cout<<"\nSold "<<item2<<"for "<<itemPListArray[2][item2IDCounter]<<" gold.";
        
        item2=empty;
        
        goto loseItemEnd;
    }
    
    if ((userInput=="item2")&&(item2==empty))
    {
        std::cout<<"\nThere's nothing there to sell!\n";
    }
    
    if ((userInput=="item3")&&(item3!=empty))
    {
        while (((itemIDArray[item3IDCounter])!=item3)&&(item3IDCounter<100))
        {
            item3IDCounter++;
        }
        
        gld=((gld+(itemPListArray[2][item3IDCounter])));
        
        std::cout<<"\nSold "<<item3<<"for "<<itemPListArray[2][item3IDCounter]<<" gold.";
        
        item3=empty;
        
        goto loseItemEnd;
    }
    
    if ((userInput=="item3")&&(item3==empty))
    {
        std::cout<<"\nThere's nothing there to sell!\n";
    }
    
    if ((userInput=="item4")&&(item4!=empty))
    {
        while (((itemIDArray[item4IDCounter])!=item4)&&(item4IDCounter<100))
        {
            item4IDCounter++;
        }
        
        gld=((gld+(itemPListArray[2][item4IDCounter])));
        
        std::cout<<"\nSold "<<item4<<"for "<<itemPListArray[2][item4IDCounter]<<" gold.";
        
        item4=empty;
        
        goto loseItemEnd;
    }
    
    if ((userInput=="item4")&&(item4==empty))
    {
        std::cout<<"\nThere's nothing there to sell!\n";
    }
    
    if ((userInput=="item5")&&(item5!=empty))
    {
        while (((itemIDArray[item5IDCounter])!=item5)&&(item5IDCounter<100))
        {
            item5IDCounter++;
        }
        
        gld=((gld+(itemPListArray[2][item5IDCounter])));
        
        std::cout<<"\nSold "<<item5<<"for "<<itemPListArray[2][item5IDCounter]<<" gold.";
        
        item5=empty;
        
        goto loseItemEnd;
    }
    
    if ((userInput=="item5")&&(item5==empty))
    {
        std::cout<<"\nThere's nothing there to sell!\n";
    }
    
    if ((userInput!="item1")&&(userInput!="item2")&&(userInput!="item3")&&(userInput!="item4")&&(userInput!="item5"))
    {
        goto loseItemq1;
    }
loseItemEnd:
    itemStats();
    return 0;
}

int quitFunc()
{
    
    return 0;
    
}


//Super-User Functions

int sudoGoTo()
{
    std::cout<<"\nx?\n";
    std::cin>>x;
    
    std::cout<<"\ny?\n";
    std::cin>>y;
    
    return 0;
}

int sudoAddGold()
{
    int gldPlus;
    
    std::cout<<"\nHow much?\n";
    std::cin>>gldPlus;
    gld=gld+gldPlus;
    std::cout<<"\n";
    
    return 0;
}

int sudoAddKey()
{
    int keyPlus;
    
    std::cout<<"\nHow many?\n";
    std::cin>>keyPlus;
    keys=keys+keyPlus;
    std::cout<<"\n";
    
    return 0;
}

int sudoAddItem()
{
    int ID;
    
    std::cout<<"\nID: ";
    std::cin>>ID;
    tempItemID=ID;
    addItem();
    std::cout<<"\n";
    
    return 0;
}

int sudoSetSkill()
{
    std::cout<<"\nEnter Skill: ";
    std::cin>>combatBoost;
    
    return 0;
}



//User Input
int input()
{
    updateSkill();
    
    std::cin>>userInput;
    
	if ((userInput==n)||(userInput==north)||(userInput==s)||(userInput==south)||(userInput==e)||(userInput==east)||(userInput==w)||(userInput==west)||(userInput==h)||(userInput==help)||(userInput==gold)||(userInput==items)||(userInput==drop)||(userInput==skill)||(userInput==wumbo)||(userInput==save)||(userInput==quit)||(userInput==sudo)||(userInput==travel)||(userInput==addgold)||(userInput==addkey)||(userInput==additems)||(userInput==setskill))
    {
        if (userInput==n||userInput==north)
        {
            y++;
            checkParams();
            if (validLocation==true)
            {}
            else
            {
                std::cout<<"\nYou cannot travel this direction any farther. The map ends.\n";
                y--;//Corrects for invalid movement
            }
        }
        
        if (userInput==s||userInput==south)
        {
            y--;
            checkParams();
            if (validLocation==true)
            {}
            else
            {
                std::cout<<"\nYou cannot travel this direction any farther. The map ends.\n";
                y++;//Corrects for invalid movement
            }
        }
        
        if (userInput==e||userInput==east)
        {
            x++;
            checkParams();
            if (validLocation==true)
            {}
            else
            {
                std::cout<<"\nYou cannot travel this direction any farther. The map ends.\n";
                x--;//Corrects for invalid movement
            }
        }
        
        if (userInput==w||userInput==west)
        {
            x--;
            checkParams();
            if (validLocation==true)
            {}
            else
            {
                std::cout<<"\nYou cannot travel this direction any farther. The map ends.\n";
                x++;//Corrects for invalid movement
            }
        }
        
        if (userInput==h||userInput==help)
        {
            std::cout<<"\n\nThese are the commands that will guide you through your journey:";
            std::cout<<"\nType 'h' or 'help' to view these at any time.";
            std::cout<<"\n\n'n' or 'north' -- travel North.";
            std::cout<<"\n's' or 'south' -- travel South.";
            std::cout<<"\n'e' or 'east' -- travel East.";
            std::cout<<"\n'w' or 'west' -- travel West.";
            std::cout<<"\n'gold' -- Check amount of gold in posession.";
            std::cout<<"\n'items' -- View inventory of items.";
            std::cout<<"\n'drop' -- Drop an item.";
            std::cout<<"\n'skill' -- Check current Skill Level.";
            std::cout<<"\n'save' -- Save current game state to load later.";
			std::cout<<"\n'quit' -- Exit the game.\n";
            std::cout<<"\n*NOTE: Reply to questions with 'yes' or 'no'.\n";
            std::cout<<"\n";
        }
        if (userInput==gold)
        {
            std::cout<<"Gold: "<<gld<<"\n";
        }
        if (userInput==items)
        {
			std::cout<<"\nInventory:\n\n1. "<<item1<<"\n2. "<<item2<<"\n3. "<<item3<<"\n4. "<<item4<<"\n5. "<<item5<<"\n"<<"Keys: "<<keys<<"\n";
        }
        
        
        if (userInput==drop)
        {
        UIitemDropq1:
            std::cout<<"\nWhich item?\n(Reply: 'item(1-5)' --- ex. item1)\n";
            std::cout<<"Inventory:\n\n1. "<<item1<<"\n2. "<<item2<<"\n3. "<<item3<<"\n4. "<<item4<<"\n5. "<<item5<<"\n";
            std::cin>>userInput;
            //
            if ((userInput=="item1")&&(item1!=empty))
            {
                std::cout<<"\nDropped "<<item1<<".";
                item1=empty;
                goto userInputEnd;
            }
            if ((userInput=="item1")&&(item1==empty))
            {
                std::cout<<"\nThere's nothing there to drop!\n";
            }
            //
            if ((userInput=="item2")&&(item2!=empty))
            {
                std::cout<<"\nDropped "<<item2<<".";
                item2=empty;
                goto userInputEnd;
            }
            if ((userInput=="item2")&&(item2==empty))
            {
                std::cout<<"\nThere's nothing there to drop!\n";
            }
            //
            if ((userInput=="item3")&&(item3!=empty))
            {
                std::cout<<"\nDropped "<<item3<<".";
                item3=empty;
                goto userInputEnd;
            }
            if ((userInput=="item3")&&(item3==empty))
            {
                std::cout<<"\nThere's nothing there to drop!\n";
            }
            //
            if ((userInput=="item4")&&(item4!=empty))
            {
                std::cout<<"\nDropped "<<item4<<".";
                item4=empty;
                goto userInputEnd;
            }
            if ((userInput=="item4")&&(item4==empty))
            {
                std::cout<<"\nThere's nothing there to drop!\n";
            }
            //
            if ((userInput=="item5")&&(item5!=empty))
            {
                std::cout<<"\nDropped "<<item5<<".";
                item5=empty;
                goto userInputEnd;
            }
            if ((userInput=="item5")&&(item5==empty))
            {
                std::cout<<"\nThere's nothing there to drop!\n";
            }
            
            if ((userInput!="item1")&&(userInput!="item2")&&(userInput!="item3")&&(userInput!="item4")&&(userInput!="item5"))
            {
                goto UIitemDropq1;
            }
        }
        
        if (userInput==skill)
        {
            std::cout<<"Skill Level: "<<playerSkill<<"\n\n";
        }
        
        if (userInput==wumbo)
        {
            std::cout<<"\nPatrick, you're an idiot...\n";
            if (wumboWandTaken==false)
            {
                std::cout<<"but here's a cool wand for trying.\n\nTake "<<wumboWand<<"?\n";
                std::cin>>userInput;
                wumboWandTaken=true;
                if (userInput==yes)
                {
                    tempItemID=5;
                    addItem();
                    itemStats();
                    goto userInputEnd;
                }
                if (userInput==no)
                {
                    std::cout<<"\n\nI don't have time for your nonsense, Patrick. Gary needs me.";
                    goto userInputEnd;
                }
                if ((userInput!=yes)&&(userInput!=no))
                {
                    std::cout<<"\n\n.......What?";
                }
                
            }
        }
        
        if (userInput==save)
        {
            saveGame();
        }
        
        if (userInput==quit)
        {
            std::cout<<"\n\nAre you sure?\n";
            std::cin>>userInput;
            
            if (userInput==yes)
            {
                std::exit(0);
            }
        }
        
        if (userInput==sudo)
        {
            if (superUser==false)
            {
                superUser=true;
                std::cout<<"\nNow in Super User mode.";
            }
            
            else
            {
                std::cout<<"\nSuper User mode already enabled.";
            }
            
        }
        
        if (superUser==false)
        {
            if (userInput==travel)
            {
                std::cout<<"\nSorry... That command is not vaild.\n";
                goto userInputEnd;
            }
            
            if (userInput==addgold)
            {
                std::cout<<"\nSorry... That command is not vaild.\n";
                goto userInputEnd;
            }
            
            if (userInput==addkey)
            {
                std::cout<<"\nSorry... That command is not vaild.\n";
                goto userInputEnd;
            }
            
            if (userInput==additems)
            {
                std::cout<<"\nSorry... That command is not vaild.\n";
                goto userInputEnd;
            }
            
            if (userInput==setskill)
            {
                std::cout<<"\nSorry... That command is not vaild.\n";
                goto userInputEnd;
            }
        }
        
        
        
        
        if (superUser==true)
        {
            if (userInput==travel)
            {
                sudoGoTo();
                goto userInputEnd;
            }
            
            if (userInput==addgold)
            {
                sudoAddGold();
                goto userInputEnd;
            }
            
            if (userInput==addkey)
            {
                sudoAddKey();
                goto userInputEnd;
            }
            
            if (userInput==additems)
            {
                sudoAddItem();
                goto userInputEnd;
            }
            
            if (userInput==setskill)
            {
                sudoSetSkill();
                goto userInputEnd;
            }
        }
    }
    
    else std::cout<<"\nSorry... That command is not vaild.\n";
    validLocation=true;
    
userInputEnd:
    return 0;
}

//Intro
int intro()
{
    std::cout<<"Greetings adventurer! Welcome to the world of Pixeless! What is your name?\n";
    std::cin>>name;
    std::cout<<"\nWell, "<<name<<", you have entered a dangerous and exciting world filled with challenges and adventures. Be ready to let your story unfold...";
    std::cout<<"\n\nThese are the commands that will guide you through your journey:";
    std::cout<<"\nType 'h' or 'help' to view these at any time.";
    std::cout<<"\n\n'n' or 'north' -- travel North.";
    std::cout<<"\n's' or 'south' -- travel South.";
    std::cout<<"\n'e' or 'east' -- travel East.";
    std::cout<<"\n'w' or 'west' -- travel West.";
    std::cout<<"\n'gold' -- Check amount of gold in posession.";
    std::cout<<"\n'items' -- View inventory of items.";
    std::cout<<"\n'drop' -- Drop an item.";
    std::cout<<"\n'skill' -- Check current Skill Level.";
    std::cout<<"\n'save' -- Save current game state to load later.";
	std::cout<<"\n'quit' -- Exit the game.\n";
    std::cout<<"\n*NOTE: Reply to questions with 'yes' or 'no' and always stick to one-word entries.\n";
    return 0;
}

//Game Over and ask to load last save
int gameOver()
{
    std::cout<<"\nYou have been defeated by your foe!";
    std::cout<<"\nLegends will tell of the Great Fallen Warrior, "<<name<<"...";
    std::cout<<"\nBetter luck next time!";
    
gameOverq1:
    std::cout<<"\n\nWould you like to load your last save?\n";
    
    std::cin>>userInput;
    
    if (userInput==yes)
    {
        std::cout<<"\n\n";
        if (saveExists==true)
        {
            loadGame();
        }
        else
        {
            std::cout<<"Can't find save.";
            goto stop;
        }
        
        goto gameOverEnd;
    }
    
    if (userInput==no)
    {
    stop:
        std::cout<<"\n\nGAME OVER";
        std::cout<<"\n\n\n****PIXELESS****\n\n";
        
        char ch;
    a:ch=std::cin.get();
        if(ch=='\n')
            exit(0);
        else
            goto a;
    }
    
    else
    {
        goto gameOverq1;
    }
gameOverEnd:
    std::cout<<"\n\n\n****PIXELESS****\n\n";
    return 0;
}

//Combat Funtions
int ratCombat()
{
    updateSkill();
    
    ratWin=false;
    
    srand(time(NULL));
    if ((rand() % 100+1)<ratWinProb+playerSkill)
    {
        if ((QuestStartl12x30==true)&&(QuestCompletel12x30==0))
        {
            QuestCounterl12x30++;
        }
        
        ratWin=true;
        combatBoost=combatBoost+ratBoost;
        std::cout<<"\nYou have defeated your foe!\n";
        
        return 1;
    }
    
    else
    {
        gameOver();
        return 0;
    }
    
}

int wolfCombat()//sword item drop
{
    updateSkill();
    
    srand(time(NULL));
    if ((rand() % 100+1)<wolfWinProb+playerSkill)
    {
        wolfWin=true;
    }
    
    if (wolfWin==true)
    {
        combatBoost=combatBoost+wolfBoost;
        std::cout<<"\nYou have defeated your foe!";
    l2x2q1:
        std::cout<<"\nIt appears to have dropped a shiny sword! \nWould you like to take it?\n";
        std::cin>>userInput;
        
        if (userInput==yes)
        {
            tempItemID=1;
            addItem();
        }
        if (userInput==no)
        {
            std::cout<<"\nYou're crazy! But okay.\n";
        }
        
        if ((userInput!=yes)&&(userInput!=no))
        {
            goto l2x2q1;
        }
    }
    
    if (wolfWin==false)
    {
        gameOver();
    }
    
wolfCombatEnd:
    return 0;
}

//Grey Dragon Combat
int greyDragCombat()//flaming helm item drop
{
    updateSkill();
    
    srand(time(NULL));
    if ((rand() % 100+1)<greyDragWinProb+playerSkill)
    {
        greyDragonWin=true;
    }
    
    if (greyDragonWin==true)
    {
        combatBoost=combatBoost+greyDragBoost;
        std::cout<<"\nAs you feel the overwhelming heat inch you closer to your demise, your life's adventures flash before your eyes...";
        std::cin.get();
        std::cout<<"\n\nBut just then, the Grey Dragon is knocked from its path!\nYou lift your head to see a mysterious hero wearing a Flaming Helm.\nHe needs your help, so you rush down to him and together, you have defeat the terrible beast!\n";
        std::cin.get();
        std::cout<<"\n\n'Thank you kind hero. You have saved me.', you tell the man.\nYou then ask 'What is your name? I must repay you.'\n";
        std::cin.get();
        std::cout<<"\nHe slowly removes his Flaming Helm...\n";
        std::cin.get();
        std::cout<<"He is... YOU.\nUtterly confused, you step back.\n""'"<<name<<", I'm you... from the future. I had to come here to save you. Your time for death has not come yet.'\n'You are gravely needed by others -- they need a hero. Once Brandon finishes the rest of the map, you must complete your destiny by traveling throughout all of Pixeless as the their valiant warrior.'\n'But I must leave at once, otherwise they will find me and kill us both.'\n'Here, take this. It will assist you in your journey.'\nHe drops his Flaming Helm on the ground. When you look back up, he is gone.";
        
    l2x5q1:
        std::cout<<"\nThe Flaming Helm rests on the ground. \nWould you like to take it?\n";
        std::cin>>userInput;
        if (userInput==yes)
        {
            tempItem=flamingHelm;
            addItem();
            if ((item1==flamingHelm)||(item2==flamingHelm)||(item3==flamingHelm)||(item4==flamingHelm)||(item5==flamingHelm))
            {
                goto greyDragCombatEnd;
            }
        }
        if (userInput==no)
        {
            std::cout<<"\nYou're crazy! But okay.\n";
        }
        
        if ((userInput!=yes)&&(userInput!=no))
        {
            goto l2x5q1;
        }
    }
    if (greyDragonWin==false)
    {
        gameOver();
    }
    
greyDragCombatEnd:
    return 0;
}

//Check Location and Location Content
int checkLocation()
{
    itemStats();
    
    //x=1************************************************************************************
    {
        if ((x==1)&&(y==2))
        {
            std::cout<<"\n\nYou are by a tree at the edge of a plain that opens up to the North...\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==3))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\nYou are in an open plain with water visible to the East...";
            
            if(checkRegister()==false)
            {
                std::cout<<"\nA rat scurries out from behind some brush and attacks!...";
                std::cout<<"\nA battle ensues!";
                std::cin.get();
                ratCombat();
                visitedRegister();
            }
            
            std::cout<<"\nWhat do you want to do?\n";
            input();
            checkLocation();
            
        }
        //****
        if ((x==1)&&(y==7))
        {
            std::cout<<"\n\n1x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==13))
        {
            std::cout<<"\n\n1x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==14))
        {
            std::cout<<"\n\n1x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==15))
        {
            std::cout<<"\n\n1x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==29))
        {
            std::cout<<"\n\n1x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==30))
        {
            std::cout<<"\n\n1x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==31))
        {
            std::cout<<"\n\n1x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==32))
        {
            std::cout<<"\n\n1x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==33))
        {
            std::cout<<"\n\n1x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==34))
        {
            std::cout<<"\n\n1x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==35))
        {
            std::cout<<"\n\n1x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==36))
        {
            std::cout<<"\n\n1x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==1)&&(y==37))
        {
            std::cout<<"\n\n1x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=2************************************************************************************
    {
        //****
        if ((x==2)&&(y==1))
        {
            std::cout<<"\n\n2x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==2))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\nYou are at the edge of a plain.\nWater is visible to the North...";
            
            if (checkRegister()==false)
            {
                std::cout<<"\nA man dressed in a brown common peasant's robe stands in the distance.\n'Hello!', he says.\n'Welcome to Pixeless!'...";
                std::cin.get();
                std::cout<<"\nSuddenly, a rabid wolf leaps from the woods and attacks the man, killing him.\nHe then turns to you and leaps forward!\nA battle ensues!";
                std::cin.get();
                wolfCombat();
                visitedRegister();
                itemStats();
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
            
        }
        //****
        if ((x==2)&&(y==3))
        {
            std::cout<<"\nYou have come to the edge of some water. It appears to be a river that flows North.\nOn the river's edge there is a stand selling fish.\n'Hi adventurer!' says a salesman at the stand.";
        l2x2q1:
            std::cout<<"\n'Would you like to buy some delicious trout?'\n";
            std::cin>>userInput;
            if (userInput==yes)
            {
            l2x2q2:
                std::cout<<"\nThey cost 5 gold a piece...\nIs that okay?\n";
                std::cin>>userInput;
                if ((userInput==yes)&&(gld>=troutbuy))
                {
                    tempItem="Trout";
                    addItem();
                    gld=gld-troutbuy;
                    goto l2x2end;
                }
                if ((userInput==yes)&&(gld<troutbuy))
                {
                    std::cout<<"\nYou don't have enough gold!\n";
                    goto l2x2end;
                }
                if (userInput==no)
                {}
                if ((userInput!=yes)&&(userInput!=no))
                {
                    goto l2x2q2;
                }
            }
            if (userInput==no)
            {
                std::cout<<"\n'That's fine', says the man.\n'Safe travels!'\n";
            }
            if ((userInput!=yes)&&(userInput!=no))
            {
                goto l2x2q1;
            }
        l2x2end:
            std::cout<<"\nWhat do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==4))
        {
            std::cout<<"\nAlong the river, there is a shelter where a few people can be seen conversing. Upon further investigation, you realize that it is a small market.\n'Greetings', says the clerk.\n'Would you like to trade?'\n";
            std::cin>>userInput;
            if (userInput==yes)
            {
                std::cout<<"\n'Great! Would you like to buy something?'\n";
                std::cin>>userInput;
                if (userInput==yes)
                {
                l2x4q3:
                    std::cout<<"\n'Okay. What would you like?'\n(Reply: 'item(#)' --- ex. item1)\nFor Sale:\nitem1: Sword (Skill Level + 2) -- "<<sword2buy<<" gold.\nitem2: Trout -- "<<troutbuy<<" gold.\n";
                    std::cin>>userInput;
                    if ((userInput=="item1")&&(gld>=sword2buy))
                    {
                        tempItem=sword2;
                        addItem();
                        gld=gld-sword2buy;
                        goto l2x4end;
                    }
                    if ((userInput=="item1")&&(gld<sword2buy))
                    {
                        std::cout<<"\nYou don't have enough gold!\n";
                    }
                    
                    if ((userInput=="item2")&&(gld>=troutbuy))
                    {
                        tempItem=trout;
                        addItem();
                        gld=gld-troutbuy;
                        goto l2x4end;
                    }
                    if ((userInput=="item2")&&(gld<troutbuy))
                    {
                        std::cout<<"\nYou don't have enough gold!\n";
                    }
                    
                    if ((userInput!="item1")&&(userInput!="item2"))
                    {
                        goto l2x4q3;
                    }
                }
                
                if (userInput==no)
                {
                    std::cout<<"\nSell something?\n";
                    std::cin>>userInput;
                    if (userInput==yes)
                    {
                        sellItem();
                    }
                    if (userInput==no)
                    {
                        std::cout<<"Okay then.\n";
                    }
                }
            }
        l2x4end:
            std::cout<<"\nWhat do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==5))
        {
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"\nAfter scaling the tallest and most dangerous mountain in all of Pixeless, you make it to the top to find what looks like a large nest.\nThen, from behind you, you hear a horrid screech! You turn around and see a large grey dragon barreling through the sky towards you! Out of its mouth spills the fire and fury of an angry mother!\nA battle ensues!";
                std::cin.get();
                greyDragCombat();
                visitedRegister();
            }
            
            else
            {
                std::cout<<"\nYou are atop the tallest and most dangerous mountain in all of Pixeless. The sky is an unsettling mix of grey and red.\n";
                std::cout<<"What do you want to do?\n";
            }
            
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==6))
        {
            std::cout<<"\n\n2x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==7))
        {
            std::cout<<"\n\n2x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==13))
        {
            std::cout<<"\n\n2x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==14))
        {
            std::cout<<"\n\n2x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==15))
        {
            std::cout<<"\n\n2x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==29))
        {
            std::cout<<"\n\n2x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==30))
        {
            std::cout<<"\n\n2x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==31))
        {
            std::cout<<"\n\n2x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==32))
        {
            std::cout<<"\n\n2x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==33))
        {
            std::cout<<"\n\n2x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==34))
        {
            std::cout<<"\n\n2x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==35))
        {
            std::cout<<"\n\n2x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==36))
        {
            std::cout<<"\n\n2x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==2)&&(y==37))
        {
            std::cout<<"\n\n2x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        
    }
    
    //x=3************************************************************************************
    {
        //****
        if ((x==3)&&(y==1))
        {
            std::cout<<"\n\n3x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==2))
        {
            std::cout<<"\n\n3x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==3))
        {
            std::cout<<"\n\n3x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==4))
        {
            std::cout<<"\n\n3x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==5))
        {
            std::cout<<"\n\n3x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==6))
        {
            std::cout<<"\n\n3x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==7))
        {
            std::cout<<"\n\n3x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==13))
        {
            std::cout<<"\n\n3x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==14))
        {
            std::cout<<"\n\n3x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==15))
        {
            std::cout<<"\n\n3x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==29))
        {
            std::cout<<"\n\n3x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==30))
        {
            std::cout<<"\n\n3x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==31))
        {
            std::cout<<"\n\n3x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==32))
        {
            std::cout<<"\n\n3x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==33))
        {
            std::cout<<"\n\n3x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==34))
        {
            std::cout<<"\n\n3x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==35))
        {
            std::cout<<"\n\n3x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==36))
        {
            std::cout<<"\n\n3x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==37))
        {
            std::cout<<"\n\n3x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==3)&&(y==38))
        {
            std::cout<<"\n\n3x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        
    }
    
    //x=4************************************************************************************
    {
        //****
        if ((x==4)&&(y==1))
        {
            std::cout<<"\n\n4x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==2))
        {
            std::cout<<"\n\n4x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==3))
        {
            std::cout<<"\n\n4x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==4))
        {
            std::cout<<"\n\n4x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==5))
        {
            std::cout<<"\n\n4x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==6))
        {
            std::cout<<"\n\n4x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==7))
        {
            std::cout<<"\n\n4x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==13))
        {
            std::cout<<"\n\n4x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==14))
        {
            std::cout<<"\n\n4x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==15))
        {
            std::cout<<"\n\n4x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==26))
        {
            std::cout<<"\n\n4x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==27))
        {
            std::cout<<"\n\n4x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==28))
        {
            std::cout<<"\n\n4x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==29))
        {
            std::cout<<"\n\n4x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==30))
        {
            std::cout<<"\n\n4x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==31))
        {
            std::cout<<"\n\n4x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==32))
        {
            std::cout<<"\n\n4x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==33))
        {
            std::cout<<"\n\n4x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==34))
        {
            std::cout<<"\n\n4x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==35))
        {
            std::cout<<"\n\n4x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==36))
        {
            std::cout<<"\n\n4x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==37))
        {
            std::cout<<"\n\n4x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==4)&&(y==38))
        {
            std::cout<<"\n\n4x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=5************************************************************************************
    {
        //****
        if ((x==5)&&(y==1))
        {
            std::cout<<"\n\n5x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==2))
        {
            std::cout<<"\n\n5x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==3))
        {
            std::cout<<"\n\n5x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==4))
        {
            std::cout<<"\n\n5x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==5))
        {
            std::cout<<"\n\n5x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==6))
        {
            std::cout<<"\n\n5x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==7))
        {
            std::cout<<"\n\n5x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==5)&&(y==8))
        {
            std::cout<<"\n\n5x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==9))
        {
            std::cout<<"\n\n5x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==13))
        {
            std::cout<<"\n\n5x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==14))
        {
            std::cout<<"\n\n5x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==15))
        {
            std::cout<<"\n\n5x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==16))
        {
            std::cout<<"\n\n5x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==17))
        {
            std::cout<<"\n\n5x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==18))
        {
            std::cout<<"\n\n5x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==26))
        {
            std::cout<<"\n\n5x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==27))
        {
            std::cout<<"\n\n5x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==28))
        {
            std::cout<<"\n\n5x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==29))
        {
            std::cout<<"\n\n5x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==30))
        {
            std::cout<<"\n\n5x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==31))
        {
            std::cout<<"\n\n5x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==32))
        {
            std::cout<<"\n\n5x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==33))
        {
            std::cout<<"\n\n5x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==34))
        {
            std::cout<<"\n\n5x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==35))
        {
            std::cout<<"\n\n5x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==36))
        {
            std::cout<<"\n\n5x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==37))
        {
            std::cout<<"\n\n5x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==5)&&(y==38))
        {
            std::cout<<"\n\n5x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=6************************************************************************************
    {
        //****
        if ((x==6)&&(y==1))
        {
            std::cout<<"\n\n6x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==2))
        {
            std::cout<<"\n\n6x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==3))
        {
            std::cout<<"\n\n6x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==4))
        {
            std::cout<<"\n\n6x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==5))
        {
            std::cout<<"\n\n6x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==6))
        {
            std::cout<<"\n\n6x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==7))
        {
            std::cout<<"\n\n6x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==6)&&(y==8))
        {
            std::cout<<"\n\n6x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==9))
        {
            std::cout<<"\n\n6x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==6)&&(y==10))
        {
            std::cout<<"\n\n6x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==6)&&(y==11))
        {
            std::cout<<"\n\n6x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==12))
        {
            std::cout<<"\n\n6x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==13))
        {
            std::cout<<"\n\n6x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==14))
        {
            std::cout<<"\n\n6x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==15))
        {
            std::cout<<"\n\n6x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==16))
        {
            std::cout<<"\n\n6x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==17))
        {
            std::cout<<"\n\n6x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==18))
        {
            std::cout<<"\n\n6x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==26))
        {
            std::cout<<"\n\n6x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==27))
        {
            std::cout<<"\n\n6x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==28))
        {
            std::cout<<"\n\n6x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==29))
        {
            std::cout<<"\n\n6x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==30))
        {
            std::cout<<"\n\n6x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==31))
        {
            std::cout<<"\n\n6x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==32))
        {
            std::cout<<"\n\n6x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==33))
        {
            std::cout<<"\n\n6x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==34))
        {
            std::cout<<"\n\n6x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==35))
        {
            std::cout<<"\n\n6x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==36))
        {
            std::cout<<"\n\n6x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==37))
        {
            std::cout<<"\n\n6x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==6)&&(y==38))
        {
            std::cout<<"\n\n6x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=7************************************************************************************
    {
        //****
        if ((x==7)&&(y==1))
        {
            std::cout<<"\n\n7x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==2))
        {
            std::cout<<"\n\n7x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==3))
        {
            std::cout<<"\n\n7x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==4))
        {
            std::cout<<"\n\n7x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==5))
        {
            std::cout<<"\n\n7x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==6))
        {
            std::cout<<"\n\n7x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==7))
        {
            std::cout<<"\n\n7x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==7)&&(y==8))
        {
            std::cout<<"\n\n7x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==9))
        {
            std::cout<<"\n\n7x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==7)&&(y==10))
        {
            std::cout<<"\n\n7x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==7)&&(y==11))
        {
            std::cout<<"\n\n7x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==12))
        {
            std::cout<<"\n\n7x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==13))
        {
            std::cout<<"\n\n7x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==14))
        {
            std::cout<<"\n\n7x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==15))
        {
            std::cout<<"\n\n7x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==16))
        {
            std::cout<<"\n\n7x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==17))
        {
            std::cout<<"\n\n7x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==18))
        {
            std::cout<<"\n\n7x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==22))
        {
            std::cout<<"\n\n7x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==23))
        {
            std::cout<<"\n\n7x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==24))
        {
            std::cout<<"\n\n7x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==25))
        {
            std::cout<<"\n\n7x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==26))
        {
            std::cout<<"\n\n7x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==27))
        {
            std::cout<<"\n\n7x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==28))
        {
            std::cout<<"\n\n7x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==29))
        {
            std::cout<<"\n\n7x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==30))
        {
            std::cout<<"\n\n7x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==31))
        {
            std::cout<<"\n\n7x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==32))
        {
            std::cout<<"\n\n7x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==33))
        {
            std::cout<<"\n\n7x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==34))
        {
            std::cout<<"\n\n7x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==35))
        {
            std::cout<<"\n\n7x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==36))
        {
            std::cout<<"\n\n7x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==37))
        {
            std::cout<<"\n\n7x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==7)&&(y==38))
        {
            std::cout<<"\n\n7x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=8************************************************************************************
    {
        //****
        if ((x==8)&&(y==1))
        {
            std::cout<<"\n\n8x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==2))
        {
            std::cout<<"\n\n8x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==3))
        {
            std::cout<<"\n\n8x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==4))
        {
            std::cout<<"\n\n8x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==5))
        {
            std::cout<<"\n\n8x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==6))
        {
            std::cout<<"\n\n8x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==7))
        {
            std::cout<<"\n\n8x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==8)&&(y==8))
        {
            std::cout<<"\n\n8x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==9))
        {
            std::cout<<"\n\n8x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==8)&&(y==10))
        {
            std::cout<<"\n\n8x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==8)&&(y==11))
        {
            std::cout<<"\n\n8x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==12))
        {
            std::cout<<"\n\n8x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==13))
        {
            std::cout<<"\n\n8x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==14))
        {
            std::cout<<"\n\n8x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==15))
        {
            std::cout<<"\n\n8x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==16))
        {
            std::cout<<"\n\n8x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==17))
        {
            std::cout<<"\n\n8x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==18))
        {
            std::cout<<"\n\n8x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==21))
        {
            std::cout<<"\n\n8x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==22))
        {
            std::cout<<"\n\n8x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==23))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x23\n";
            std::cout<<"\n\nYou have come close the middle of a murkey area.\nFrogs and crickets are creating a subtle hum in the background... or is that the river?.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==24))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x24\n";
            std::cout<<"\n\nThis is the South Western valley under the hill that Serren sits upon.\nIt is a peaceful grassy slope with scattered trees and bushes.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==25))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x25\n";
            std::cout<<"\n\nThe South Western corner of the town walls are within reach.\nA tower stands tall and guards armed with bows stand ready.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==26))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x26\n";
            std::cout<<"\n\nYou are outside the corner of Serren.\n";
            
            if (checkRegister()==false)
            {
                std::cout<<"\n\nThere is a sewer drain coming from the city walls.\nA strange gleam catches your eye and your curiosity gets the better of you.\nAfter searching the drain, you find a key!\nAdded key.\n";
				keys++;
				visitedRegister();
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==27))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x27\n";
            std::cout<<"\n\nFrom here you can see the smoke from your chimeny(sp?) to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==28))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x28\n";
            std::cout<<"\n\nThis is near the crest of the Hill.\nThe town of Serren is to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==29))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x29\n";
            std::cout<<"\n\nYou are on top of the large hill near the city. You can see so far from here...\nLooks like there is a large ship yard to the South West.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==30))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x30\n";
            std::cout<<"\n\nThis is a grass plain on the north side of the hill.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==31))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x31\n";
            std::cout<<"\n\nYou can hear a muffled conversation between a group of guards..\nThey must be on break near here.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==32))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x32\n";
            std::cout<<"\n\nThe North side of the hill is a bit darker than the Southern side.\nThe city blocks much of the sunlight.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==33))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n8x33\n";
            std::cout<<"\n\nHere is the North Western corner of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==34))
        {
            std::cout<<"\n\n8x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==35))
        {
            std::cout<<"\n\n8x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==8)&&(y==36))
        {
            std::cout<<"\n\n8x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=9************************************************************************************
    {
        //****
        if ((x==9)&&(y==3))
        {
            std::cout<<"\n\n9x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==4))
        {
            std::cout<<"\n\n9x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==5))
        {
            std::cout<<"\n\n9x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==6))
        {
            std::cout<<"\n\n9x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==7))
        {
            std::cout<<"\n\n9x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==9)&&(y==8))
        {
            std::cout<<"\n\n9x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==9))
        {
            std::cout<<"\n\n9x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==9)&&(y==10))
        {
            std::cout<<"\n\n9x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==9)&&(y==11))
        {
            std::cout<<"\n\n9x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==12))
        {
            std::cout<<"\n\n9x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==13))
        {
            std::cout<<"\n\n9x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==14))
        {
            std::cout<<"\n\n9x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==15))
        {
            std::cout<<"\n\n9x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==16))
        {
            std::cout<<"\n\n9x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==17))
        {
            std::cout<<"\n\n9x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==18))
        {
            std::cout<<"\n\n9x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==21))
        {
            std::cout<<"\n\n9x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==22))
        {
            std::cout<<"\n\n9x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==23))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x23\n";
            std::cout<<"\n\nThere is a road to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==24))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x24\n";
            std::cout<<"\n\nYou are at the South Western outskirts of the town.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==25))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x25\n";
            std::cout<<"\n\nThis is the Western corner of the entrance to Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==26))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x26\n";
            std::cout<<"\n\nYou are in the Southwester corner of Serren, directly south of your house.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==27))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x27\n";
            std::cout<<"\n\nThis is your house, a quaint cottage in the Southwestern part of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==28))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x28\n";
            std::cout<<"\n\nYou are at the lower West side of Serren. Your house is to the south.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==29))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x29\n";
            std::cout<<"\n\nHere is a town resident's home. You cannot go in.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==30))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x30\n";
            std::cout<<"\n\nYou are at the upper West side of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==31))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x31\n";
            std::cout<<"\n\nAt a large building there are guards going in and out. A sign above the door reads 'Serren Guard Housing'\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==32))
        {
            locRegister();
            checkRegister();
            
            
            std::cout<<"\n\n9x32\n";
            std::cout<<"\n\nYou are in the Northwester corner of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==33))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==34))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==35))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==9)&&(y==36))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n9x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=10***********************************************************************************
    {
        //****
        if ((x==10)&&(y==3))
        {
            std::cout<<"\n\n10x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==4))
        {
            std::cout<<"\n\n10x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==5))
        {
            std::cout<<"\n\n10x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==6))
        {
            std::cout<<"\n\n10x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==7))
        {
            std::cout<<"\n\n10x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==10)&&(y==8))
        {
            std::cout<<"\n\n10x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==9))
        {
            std::cout<<"\n\n10x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==10)&&(y==10))
        {
            std::cout<<"\n\n10x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==10)&&(y==11))
        {
            std::cout<<"\n\n10x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==12))
        {
            std::cout<<"\n\n10x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==13))
        {
            std::cout<<"\n\n10x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==14))
        {
            std::cout<<"\n\n10x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==15))
        {
            std::cout<<"\n\n10x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==16))
        {
            std::cout<<"\n\n10x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==17))
        {
            std::cout<<"\n\n10x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==18))
        {
            std::cout<<"\n\n10x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==10)&&(y==19))
        {
            std::cout<<"\n\n10x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==10)&&(y==20))
        {
            std::cout<<"\n\n10x20\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==21))
        {
            std::cout<<"\n\n10x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==22))
        {
            std::cout<<"\n\n10x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==23))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x23\n";
            std::cout<<"\n\nThe path you're on goes North and South.\nYou can see a large town sitting on top of a hill to the North.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==24))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x24\n";
            std::cout<<"\n\nHere, a group of Serren town guards and horses seem to be anxiously awaiting something.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==25))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==26))
        {
            std::cout<<"\n\n10x26\n";
            std::cout<<"\n\nYou are in the Southwestern part of Serren. Your house can be seen to the Northwest.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==27))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x27\n";
            std::cout<<"\n\nYour house is to the West. You hear a busy cluster of people near by.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==28))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x28\n";
            std::cout<<"\n\nYou are near the Western side of town. A man nearby says to you,\n'Have you visited the local weapons shop just East of here? If you plan on venturing outside the safety of the city, acquiring a weapon could save your life.'\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==29))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x29\n";
            std::cout<<"\n\nThis is the west side of Serren. Town Square is to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==30))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x30\n";
            std::cout<<"\n\nYou are at the West side of town.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==31))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x31\n";
            std::cout<<"\n\nYou have come to the Northwestern side of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==32))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n10x32\n";
            std::cout<<"\n\nYou are at the corner of town. You can see the Serren Castle entrance to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==33))
        {
            std::cout<<"\n\n10x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==34))
        {
            std::cout<<"\n\n10x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==35))
        {
            std::cout<<"\n\n10x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==10)&&(y==36))
        {
            std::cout<<"\n\n10x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=11***********************************************************************************
    {
        //****
        if ((x==11)&&(y==3))
        {
            std::cout<<"\n\n11x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==4))
        {
            std::cout<<"\n\n11x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==5))
        {
            std::cout<<"\n\n11x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==6))
        {
            std::cout<<"\n\n11x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==7))
        {
            std::cout<<"\n\n11x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==11)&&(y==8))
        {
            std::cout<<"\n\n11x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==9))
        {
            std::cout<<"\n\n11x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==11)&&(y==10))
        {
            std::cout<<"\n\n11x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==11)&&(y==11))
        {
            std::cout<<"\n\n11x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==12))
        {
            std::cout<<"\n\n11x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==13))
        {
            std::cout<<"\n\n11x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==14))
        {
            std::cout<<"\n\n11x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==15))
        {
            std::cout<<"\n\n11x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==16))
        {
            std::cout<<"\n\n11x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==17))
        {
            std::cout<<"\n\n11x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==18))
        {
            std::cout<<"\n\n11x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==21))
        {
            std::cout<<"\n\n11x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==22))
        {
            std::cout<<"\n\n11x22\n";
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==23))
        {
            std::cout<<"\n\n11x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==24))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x24\n";
            std::cout<<"\n\nThis is the Southern edge of Serren. A road leading out of the city heads out to the West.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==25))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x25\n";
            std::cout<<"\n\nYou have come to the Southwestern corner of the town.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==26))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x26\n";
            std::cout<<"\n\nYou are at the Southwestern area of Serre. You can see a busy Town Square up ahead to the Northeast.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==27))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x27\n";
            std::cout<<"\n\nYou are close to Town Square. A man asks you, 'Did you know that weapons and armor will help you in battle by increasing your Skill Level?\nCheck the stores around Town Square to find items that may help you.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==28))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x28\n";
            std::cout<<"\n\n'Hello!'\nYou have come to a small shop in the Serren Town Square.\n'Welcome to the Serren Weapon shop.'\n'Would you like to trade?'\n";
            std::cin>>userInput;
            if (userInput==yes)
            {
                std::cout<<"\n'Great! Would you like to buy something?'\n";
                std::cin>>userInput;
                if (userInput==yes)
                {
                l11x28q1:
                    std::cout<<"\n'Okay. What would you like?'\n(Reply: 'item(#)' --- ex. item1)\nFor Sale:\nitem1: "<<itemIDArray[1]<<" -- "<<itemPListArray[1][1]<<" gold.\nitem2: "<<itemIDArray[2]<<" -- "<<itemPListArray[1][2]<<" gold.\nitem3: "<<itemIDArray[3]<<" -- "<<itemPListArray[1][3]<<" gold.\n";
                    std::cin>>userInput;
                    
                    
                    
                    if ((userInput=="item1")&&(gld>=itemPListArray[1][1]))
                    {
                        tempItemID=1;
                        addItem();
                        gld=gld-itemPListArray[1][1];
                        goto l11x28End;
                    }
                    if ((userInput=="item1")&&(gld<itemPListArray[1][1]))
                    {
                        std::cout<<"\nYou don't have enough gold!\n";
                    }
                    
                    
                    if ((userInput=="item2")&&(gld>=itemPListArray[1][2]))
                    {
                        tempItemID=2;
                        addItem();
                        gld=gld-itemPListArray[1][2];
                        goto l11x28End;
                    }
                    if ((userInput=="item2")&&(gld<itemPListArray[1][2]))
                    {
                        std::cout<<"\nYou don't have enough gold!\n";
                    }
                    
                    
                    if ((userInput=="item3")&&(gld>=itemPListArray[1][3]))
                    {
                        tempItemID=3;
                        addItem();
                        gld=gld-itemPListArray[1][3];
                        goto l11x28End;
                    }
                    if ((userInput=="item3")&&(gld<itemPListArray[1][3]))
                    {
                        std::cout<<"\nYou don't have enough gold!\n";
                    }
                    
                    
                    if ((userInput!="item1")&&(userInput!="item2")&&(userInput!="item3"))
                    {
                        goto l11x28q1;
                    }
                }
                
                if (userInput==no)
                {
                    std::cout<<"\nSell something?\n";
                    std::cin>>userInput;
                    if (userInput==yes)
                    {
                        sellItem();
                    }
                    if (userInput==no)
                    {
                        std::cout<<"'Okay then.'\n";
                    }
                }
            }
        l11x28End:
            std::cout<<"\nWhat do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==29))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x29\n";
            std::cout<<"\n\nTown Square isn't too far off. There is a shop to the South.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==30))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x30\n";
            std::cout<<"\n\nThe Serren Castle is visible in the distance. It appears to be about a mile due North. You are near the Northwestern part of town.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==31))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x31\n";
            std::cout<<"\n\nIt is obvious that you are near the castle due to the increasing amount of guards patrolling the area.\nOne of the armoured guards approaches you and says,\n'Good day! You appear to be quite capable. Check around Serren and I'm sure you'll find some residents with quests you could help them with. They offer good rewards!'\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==32))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x32\n";
            std::cout<<"\n\nYou are at the Northwestern corner of Serren. The town's elaborate castle entrance is to the east.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==33))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n11x33\n";
            std::cout<<"\n\nThis is a towering statue made of gold in the image of the first queen of Serren named Aesthenia. It is said that she was brought to the Founding King, Thyron, by the gods on a silver canoe floating on the Strillow River.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==34))
        {
            std::cout<<"\n\n11x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==35))
        {
            std::cout<<"\n\n11x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==11)&&(y==36))
        {
            std::cout<<"\n\n11x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        
    }
    
    //x=12***********************************************************************************
    {
        //****
        if ((x==12)&&(y==3))
        {
            std::cout<<"\n\n12x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==4))
        {
            std::cout<<"\n\n12x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==5))
        {
            std::cout<<"\n\n12x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==6))
        {
            std::cout<<"\n\n12x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==7))
        {
            std::cout<<"\n\n12x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==12)&&(y==8))
        {
            std::cout<<"\n\n12x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==9))
        {
            std::cout<<"\n\n12x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==12)&&(y==10))
        {
            std::cout<<"\n\n12x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==12)&&(y==11))
        {
            std::cout<<"\n\n12x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==12))
        {
            std::cout<<"\n\n12x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==13))
        {
            std::cout<<"\n\n12x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==14))
        {
            std::cout<<"\n\n12x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==15))
        {
            std::cout<<"\n\n12x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==16))
        {
            std::cout<<"\n\n12x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==17))
        {
            std::cout<<"\n\n12x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==18))
        {
            std::cout<<"\n\n12x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==22))
        {
            std::cout<<"\n\n12x22\n";
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"A rat attacks!\n";
                ratCombat();
                visitedRegister();
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==23))
        {
            std::cout<<"\n\n12x23\n";
            
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"A rat attacks!\n";
                ratCombat();
                visitedRegister();
            }
            
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
            
            
            
        }
        //****
        if ((x==12)&&(y==24))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x24\n";
            std::cout<<"\n\nNear the southern entrance to Serren, you overhear a couple discussing the city's security.\n'I am in love with this town!' says one of them.\nThe other replies, 'I agree! We're lucky to have such a well-trained guard force to protect us.'\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==25))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x25\n";
            std::cout<<"\n\nYou are at the Southern side of Serren.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==26))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x26\n";
            std::cout<<"\n\nThis is the Southwestern area of town. There is a large group of citizens just North of here.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==27))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x27\n";
            std::cout<<"\n\nYou are amidst many Serren residents in the Serren Town Square. Water gushes from a beautiful fountain to the Northeast.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==28))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x28\n";
            std::cout<<"\n\nTown Square is overflowing with the people of Serren. There is a large fountain to the East.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==29))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x29\n";
            std::cout<<"\n\nYou are near the middle of town. You can see more people and a few shops on the Eastern side, past the fountain.\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==30))
        {
            locRegister();
            checkRegister();
            
            std::cout<<"\n\n12x30\n";
            
            if (QuestStartl12x30==0)
            {
				if (QuestCounterl12x30>=5)
				{
					QuestCompletel12x30=1;
				}
                
                std::cout<<"\n\nAt the Northern side of Town Square, a guard makes his way up to you.\n'Why hello! Would you mind helping the guards of Serren with a task? Citizens have been complaining about hostile rats roaming the plains around the city. We have more important things to manage, and need someone to kill 5 rats.'";
            l12x30q1:
                std::cout<<"\n'What do you say? We will reward you for your service!'\n";
                
                std::cin>>userInput;
                
                if (userInput==yes)
                {
                    QuestStartl12x30=true;
                    std::cout<<"\n\n'Great! Come back and see me when you've defeated 5 rats.\n";
                }
                
                if (userInput==no)
                {
                    std::cout<<"\n\n'Argh.. Oh well.'\n";
                }
                
                if ((userInput!=yes)&&(userInput!=no))
                {
                    goto l12x30q1;
                }
            }
            
            else
            {
                if (QuestCompletel12x30==0)
                {
                    std::cout<<"\nNear the Northern side of the plaza, the guard from before asks,\n'How's it going with those rats?'\n'Keep at it.'\n";
                }
                
                else if (QuestCompletel12x30==1)
                {
                    std::cout<<"\n'How's it going with those rats?'\n'Oh you did it, fantastic! You've really helped us guards out! As promised, here's your reward.'\nHe gives you an Honorary Guard's Shield (+2 Skill Level).\n";
                    QuestCompletel12x30=2;
                    QuestCounterl12x30--;
                    tempItemID=6;
                    addItem();
                }
                
                else if (QuestCompletel12x30==2)
                {
                    std::cout<<"\n\nYou are at the Northern side of Town Square. The guard you assisted before says, 'Greetings, "<<name<<"! Thanks for your help!'\n";
                }
                
                else
                {
                    std::cout<<"\n\nYou are at the Northern side of Town Square. The guard you assisted before says, 'Greetings, "<<name<<"! Thanks for your help!'\n";
                }
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==31))
        {
            std::cout<<"\n\n12x31\n";
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==32))
        {
            std::cout<<"\n\n12x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==33))
        {
            std::cout<<"\n\n12x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==34))
        {
            std::cout<<"\n\n12x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==35))
        {
            std::cout<<"\n\n12x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==36))
        {
            std::cout<<"\n\n12x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==37))
        {
            std::cout<<"\n\n12x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==38))
        {
            std::cout<<"\n\n12x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==39))
        {
            std::cout<<"\n\n12x39\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==12)&&(y==40))
        {
            std::cout<<"\n\n12x40\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=13***********************************************************************************
    {
        //****
        if ((x==13)&&(y==2))
        {
            std::cout<<"\n\n13x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==3))
        {
            std::cout<<"\n\n13x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==4))
        {
            std::cout<<"\n\n13x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==5))
        {
            std::cout<<"\n\n13x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==6))
        {
            std::cout<<"\n\n13x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==7))
        {
            std::cout<<"\n\n13x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==13)&&(y==8))
        {
            std::cout<<"\n\n13x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==9))
        {
            std::cout<<"\n\n13x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==13)&&(y==10))
        {
            std::cout<<"\n\n13x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==13)&&(y==11))
        {
            std::cout<<"\n\n13x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==12))
        {
            std::cout<<"\n\n13x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==13))
        {
            std::cout<<"\n\n13x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==14))
        {
            std::cout<<"\n\n13x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==15))
        {
            std::cout<<"\n\n13x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==16))
        {
            std::cout<<"\n\n13x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==17))
        {
            std::cout<<"\n\n13x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==18))
        {
            std::cout<<"\n\n13x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==13)&&(y==19))
        {
            std::cout<<"\n\n13x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==22))
        {
            std::cout<<"\n\n13x22\n";
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"A rat attacks!\n";
                ratCombat();
                visitedRegister();
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==23))
        {
            std::cout<<"\n\n13x23\n";
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"A rat attacks!\n";
                ratCombat();
                visitedRegister();
            }
            
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==24))
        {
            std::cout<<"\n\n13x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==25))
        {
            std::cout<<"\n\n13x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==26))
        {
            std::cout<<"\n\n13x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==27))
        {
            std::cout<<"\n\n13x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==28))
        {
            std::cout<<"\n\n13x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==29))
        {
            std::cout<<"\n\n13x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==30))
        {
            std::cout<<"\n\n13x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==31))
        {
            std::cout<<"\n\n13x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==32))
        {
            std::cout<<"\n\n13x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==33))
        {
            std::cout<<"\n\n13x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==34))
        {
            std::cout<<"\n\n13x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==35))
        {
            std::cout<<"\n\n13x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==36))
        {
            std::cout<<"\n\n13x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==37))
        {
            std::cout<<"\n\n13x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==38))
        {
            std::cout<<"\n\n13x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==13)&&(y==39))
        {
            std::cout<<"\n\n13x39\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        
    }
    
    //x=14***********************************************************************************
    {
        //****
        if ((x==14)&&(y==1))
        {
            std::cout<<"\n\n14x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==2))
        {
            std::cout<<"\n\n14x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==3))
        {
            std::cout<<"\n\n14x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==4))
        {
            std::cout<<"\n\n14x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==5))
        {
            std::cout<<"\n\n14x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==6))
        {
            std::cout<<"\n\n14x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==7))
        {
            std::cout<<"\n\n14x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==14)&&(y==8))
        {
            std::cout<<"\n\n14x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==9))
        {
            std::cout<<"\n\n14x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==14)&&(y==10))
        {
            std::cout<<"\n\n14x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==14)&&(y==11))
        {
            std::cout<<"\n\n14x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==12))
        {
            std::cout<<"\n\n14x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==13))
        {
            std::cout<<"\n\n14x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==14))
        {
            std::cout<<"\n\n14x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==15))
        {
            std::cout<<"\n\n14x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==16))
        {
            std::cout<<"\n\n14x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==17))
        {
            std::cout<<"\n\n14x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==18))
        {
            std::cout<<"\n\n14x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==22))
        {
            std::cout<<"\n\n14x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==23))
        {
            std::cout<<"\n\n14x23\n";
            locRegister();
            checkRegister();
            
            if (checkRegister()==false)
            {
                std::cout<<"A rat attacks!\n";
                ratCombat();
                visitedRegister();
            }
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==24))
        {
            std::cout<<"\n\n14x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==25))
        {
            std::cout<<"\n\n14x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==26))
        {
            std::cout<<"\n\n14x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==27))
        {
            std::cout<<"\n\n14x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==28))
        {
            std::cout<<"\n\n14x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==29))
        {
            std::cout<<"\n\n14x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==30))
        {
            std::cout<<"\n\n14x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==31))
        {
            std::cout<<"\n\n14x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==32))
        {
            std::cout<<"\n\n14x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==33))
        {
            std::cout<<"\n\n14x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==34))
        {
            std::cout<<"\n\n14x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==35))
        {
            std::cout<<"\n\n14x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==36))
        {
            std::cout<<"\n\n14x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==37))
        {
            std::cout<<"\n\n14x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==14)&&(y==38))
        {
            std::cout<<"\n\n14x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=15***********************************************************************************
    {
        //****
        if ((x==15)&&(y==1))
        {
            std::cout<<"\n\n15x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==2))
        {
            std::cout<<"\n\n15x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==3))
        {
            std::cout<<"\n\n15x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==4))
        {
            std::cout<<"\n\n15x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==5))
        {
            std::cout<<"\n\n15x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==6))
        {
            std::cout<<"\n\n15x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==7))
        {
            std::cout<<"\n\n15x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==15)&&(y==8))
        {
            std::cout<<"\n\n15x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==9))
        {
            std::cout<<"\n\n15x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==15)&&(y==10))
        {
            std::cout<<"\n\n15x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==15)&&(y==11))
        {
            std::cout<<"\n\n15x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==12))
        {
            std::cout<<"\n\n15x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==13))
        {
            std::cout<<"\n\n15x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==14))
        {
            std::cout<<"\n\n15x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==15))
        {
            std::cout<<"\n\n15x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==16))
        {
            std::cout<<"\n\n15x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==17))
        {
            std::cout<<"\n\n15x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==18))
        {
            std::cout<<"\n\n15x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==21))
        {
            std::cout<<"\n\n15x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==22))
        {
            std::cout<<"\n\n15x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==23))
        {
            std::cout<<"\n\n15x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==24))
        {
            std::cout<<"\n\n15x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==25))
        {
            std::cout<<"\n\n15x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==26))
        {
            std::cout<<"\n\n15x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==27))
        {
            std::cout<<"\n\n15x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==28))
        {
            std::cout<<"\n\n15x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==29))
        {
            std::cout<<"\n\n15x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==30))
        {
            std::cout<<"\n\n15x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==31))
        {
            std::cout<<"\n\n15x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==32))
        {
            std::cout<<"\n\n15x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==33))
        {
            std::cout<<"\n\n15x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==34))
        {
            std::cout<<"\n\n15x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==35))
        {
            std::cout<<"\n\n15x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==36))
        {
            std::cout<<"\n\n15x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==37))
        {
            std::cout<<"\n\n15x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==15)&&(y==38))
        {
            std::cout<<"\n\n15x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=16***********************************************************************************
    {
        //****
        if ((x==16)&&(y==1))
        {
            std::cout<<"\n\n16x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==2))
        {
            std::cout<<"\n\n16x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==3))
        {
            std::cout<<"\n\n16x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==4))
        {
            std::cout<<"\n\n16x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==5))
        {
            std::cout<<"\n\n16x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==6))
        {
            std::cout<<"\n\n16x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==7))
        {
            std::cout<<"\n\n16x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==16)&&(y==8))
        {
            std::cout<<"\n\n16x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==9))
        {
            std::cout<<"\n\n16x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==16)&&(y==10))
        {
            std::cout<<"\n\n16x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==16)&&(y==11))
        {
            std::cout<<"\n\n16x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==12))
        {
            std::cout<<"\n\n16x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==13))
        {
            std::cout<<"\n\n16x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==14))
        {
            std::cout<<"\n\n16x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==15))
        {
            std::cout<<"\n\n16x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==16))
        {
            std::cout<<"\n\n16x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==17))
        {
            std::cout<<"\n\n16x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==18))
        {
            std::cout<<"\n\n16x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==19))
        {
            std::cout<<"\n\n16x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==20))
        {
            std::cout<<"\n\n16x20\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==21))
        {
            std::cout<<"\n\n16x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==22))
        {
            std::cout<<"\n\n16x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==23))
        {
            std::cout<<"\n\n16x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==24))
        {
            std::cout<<"\n\n16x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==25))
        {
            std::cout<<"\n\n16x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==26))
        {
            std::cout<<"\n\n16x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==27))
        {
            std::cout<<"\n\n16x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==28))
        {
            std::cout<<"\n\n16x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==29))
        {
            std::cout<<"\n\n16x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==30))
        {
            std::cout<<"\n\n16x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==31))
        {
            std::cout<<"\n\n16x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==32))
        {
            std::cout<<"\n\n16x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==33))
        {
            std::cout<<"\n\n16x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==34))
        {
            std::cout<<"\n\n16x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==35))
        {
            std::cout<<"\n\n16x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==36))
        {
            std::cout<<"\n\n16x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==37))
        {
            std::cout<<"\n\n16x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==16)&&(y==38))
        {
            std::cout<<"\n\n16x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=17***********************************************************************************
    {
        //****
        if ((x==17)&&(y==1))
        {
            std::cout<<"\n\n17x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==2))
        {
            std::cout<<"\n\n17x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==3))
        {
            std::cout<<"\n\n17x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==4))
        {
            std::cout<<"\n\n17x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==5))
        {
            std::cout<<"\n\n17x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==6))
        {
            std::cout<<"\n\n17x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==7))
        {
            std::cout<<"\n\n17x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==17)&&(y==8))
        {
            std::cout<<"\n\n17x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==9))
        {
            std::cout<<"\n\n17x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==17)&&(y==10))
        {
            std::cout<<"\n\n17x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==17)&&(y==11))
        {
            std::cout<<"\n\n17x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==12))
        {
            std::cout<<"\n\n17x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==13))
        {
            std::cout<<"\n\n17x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==14))
        {
            std::cout<<"\n\n17x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==15))
        {
            std::cout<<"\n\n17x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==16))
        {
            std::cout<<"\n\n17x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==17))
        {
            std::cout<<"\n\n17x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==21))
        {
            std::cout<<"\n\n17x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==22))
        {
            std::cout<<"\n\n17x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==23))
        {
            std::cout<<"\n\n17x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==24))
        {
            std::cout<<"\n\n17x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==25))
        {
            std::cout<<"\n\n17x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==26))
        {
            std::cout<<"\n\n17x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==27))
        {
            std::cout<<"\n\n17x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==28))
        {
            std::cout<<"\n\n17x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==29))
        {
            std::cout<<"\n\n17x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==30))
        {
            std::cout<<"\n\n17x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==31))
        {
            std::cout<<"\n\n17x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==32))
        {
            std::cout<<"\n\n17x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==33))
        {
            std::cout<<"\n\n17x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==34))
        {
            std::cout<<"\n\n17x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==35))
        {
            std::cout<<"\n\n17x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==36))
        {
            std::cout<<"\n\n17x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==37))
        {
            std::cout<<"\n\n17x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==17)&&(y==38))
        {
            std::cout<<"\n\n17x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=18***********************************************************************************
    {
        //****
        if ((x==18)&&(y==1))
        {
            std::cout<<"\n\n18x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==2))
        {
            std::cout<<"\n\n18x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==3))
        {
            std::cout<<"\n\n18x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==4))
        {
            std::cout<<"\n\n18x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==5))
        {
            std::cout<<"\n\n18x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==6))
        {
            std::cout<<"\n\n18x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==7))
        {
            std::cout<<"\n\n18x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==18)&&(y==8))
        {
            std::cout<<"\n\n18x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==9))
        {
            std::cout<<"\n\n18x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==18)&&(y==10))
        {
            std::cout<<"\n\n18x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==18)&&(y==11))
        {
            std::cout<<"\n\n18x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==12))
        {
            std::cout<<"\n\n18x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==13))
        {
            std::cout<<"\n\n18x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==14))
        {
            std::cout<<"\n\n18x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==15))
        {
            std::cout<<"\n\n18x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==16))
        {
            std::cout<<"\n\n18x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==17))
        {
            std::cout<<"\n\n18x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==21))
        {
            std::cout<<"\n\n18x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==22))
        {
            std::cout<<"\n\n18x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==23))
        {
            std::cout<<"\n\n18x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==24))
        {
            std::cout<<"\n\n18x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==25))
        {
            std::cout<<"\n\n18x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==26))
        {
            std::cout<<"\n\n18x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==27))
        {
            std::cout<<"\n\n18x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==28))
        {
            std::cout<<"\n\n18x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==29))
        {
            std::cout<<"\n\n18x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==30))
        {
            std::cout<<"\n\n18x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==31))
        {
            std::cout<<"\n\n18x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==32))
        {
            std::cout<<"\n\n18x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==33))
        {
            std::cout<<"\n\n18x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==34))
        {
            std::cout<<"\n\n18x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==35))
        {
            std::cout<<"\n\n18x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==36))
        {
            std::cout<<"\n\n18x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==37))
        {
            std::cout<<"\n\n18x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==18)&&(y==38))
        {
            std::cout<<"\n\n18x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=19***********************************************************************************
    {
        //****
        if ((x==19)&&(y==1))
        {
            std::cout<<"\n\n19x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==2))
        {
            std::cout<<"\n\n19x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==3))
        {
            std::cout<<"\n\n19x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==4))
        {
            std::cout<<"\n\n19x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==5))
        {
            std::cout<<"\n\n19x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==6))
        {
            std::cout<<"\n\n19x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==7))
        {
            std::cout<<"\n\n19x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==19)&&(y==8))
        {
            std::cout<<"\n\n19x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==9))
        {
            std::cout<<"\n\n19x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==19)&&(y==10))
        {
            std::cout<<"\n\n19x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==19)&&(y==11))
        {
            std::cout<<"\n\n19x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==12))
        {
            std::cout<<"\n\n19x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==13))
        {
            std::cout<<"\n\n19x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==14))
        {
            std::cout<<"\n\n19x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==15))
        {
            std::cout<<"\n\n19x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==16))
        {
            std::cout<<"\n\n19x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==17))
        {
            std::cout<<"\n\n19x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==21))
        {
            std::cout<<"\n\n19x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==22))
        {
            std::cout<<"\n\n19x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==23))
        {
            std::cout<<"\n\n19x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==24))
        {
            std::cout<<"\n\n19x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==25))
        {
            std::cout<<"\n\n19x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==26))
        {
            std::cout<<"\n\n19x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==27))
        {
            std::cout<<"\n\n19x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==28))
        {
            std::cout<<"\n\n19x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==29))
        {
            std::cout<<"\n\n19x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==30))
        {
            std::cout<<"\n\n19x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==31))
        {
            std::cout<<"\n\n19x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==32))
        {
            std::cout<<"\n\n19x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==33))
        {
            std::cout<<"\n\n19x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==34))
        {
            std::cout<<"\n\n19x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==35))
        {
            std::cout<<"\n\n19x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==36))
        {
            std::cout<<"\n\n19x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==37))
        {
            std::cout<<"\n\n19x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==38))
        {
            std::cout<<"\n\n19x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==19)&&(y==39))
        {
            std::cout<<"\n\n19x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=20***********************************************************************************
    {
        //****
        if ((x==20)&&(y==1))
        {
            std::cout<<"\n\n20x1\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==2))
        {
            std::cout<<"\n\n20x2\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==3))
        {
            std::cout<<"\n\n20x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==4))
        {
            std::cout<<"\n\n20x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==5))
        {
            std::cout<<"\n\n20x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==6))
        {
            std::cout<<"\n\n20x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==7))
        {
            std::cout<<"\n\n20x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==20)&&(y==8))
        {
            std::cout<<"\n\n20x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==9))
        {
            std::cout<<"\n\n20x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==20)&&(y==10))
        {
            std::cout<<"\n\n20x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==20)&&(y==11))
        {
            std::cout<<"\n\n20x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==12))
        {
            std::cout<<"\n\n20x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==13))
        {
            std::cout<<"\n\n20x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==14))
        {
            std::cout<<"\n\n20x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==15))
        {
            std::cout<<"\n\n20x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==16))
        {
            std::cout<<"\n\n20x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==17))
        {
            std::cout<<"\n\n20x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==21))
        {
            std::cout<<"\n\n20x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==22))
        {
            std::cout<<"\n\n20x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==23))
        {
            std::cout<<"\n\n20x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==24))
        {
            std::cout<<"\n\n20x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==25))
        {
            std::cout<<"\n\n20x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==26))
        {
            std::cout<<"\n\n20x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==27))
        {
            std::cout<<"\n\n20x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==28))
        {
            std::cout<<"\n\n20x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==29))
        {
            std::cout<<"\n\n20x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==30))
        {
            std::cout<<"\n\n20x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==31))
        {
            std::cout<<"\n\n20x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==32))
        {
            std::cout<<"\n\n20x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==33))
        {
            std::cout<<"\n\n20x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==20)&&(y==34))
        {
            std::cout<<"\n\n20x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=21***********************************************************************************
    {
        //****
        if ((x==21)&&(y==3))
        {
            std::cout<<"\n\n21x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==4))
        {
            std::cout<<"\n\n21x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==5))
        {
            std::cout<<"\n\n21x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==6))
        {
            std::cout<<"\n\n21x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==7))
        {
            std::cout<<"\n\n21x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==21)&&(y==8))
        {
            std::cout<<"\n\n21x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==9))
        {
            std::cout<<"\n\n21x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==21)&&(y==10))
        {
            std::cout<<"\n\n21x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==21)&&(y==11))
        {
            std::cout<<"\n\n21x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==12))
        {
            std::cout<<"\n\n21x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==13))
        {
            std::cout<<"\n\n21x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==14))
        {
            std::cout<<"\n\n21x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==15))
        {
            std::cout<<"\n\n21x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==16))
        {
            std::cout<<"\n\n21x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==17))
        {
            std::cout<<"\n\n21x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==21))
        {
            std::cout<<"\n\n21x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==22))
        {
            std::cout<<"\n\n21x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==23))
        {
            std::cout<<"\n\n21x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==24))
        {
            std::cout<<"\n\n21x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==25))
        {
            std::cout<<"\n\n21x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==26))
        {
            std::cout<<"\n\n21x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==27))
        {
            std::cout<<"\n\n21x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==28))
        {
            std::cout<<"\n\n21x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==29))
        {
            std::cout<<"\n\n21x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==30))
        {
            std::cout<<"\n\n21x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==31))
        {
            std::cout<<"\n\n21x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==32))
        {
            std::cout<<"\n\n21x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==33))
        {
            std::cout<<"\n\n21x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==21)&&(y==34))
        {
            std::cout<<"\n\n21x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=22***********************************************************************************
    {
        //****
        if ((x==22)&&(y==3))
        {
            std::cout<<"\n\n22x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==4))
        {
            std::cout<<"\n\n22x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==5))
        {
            std::cout<<"\n\n22x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==6))
        {
            std::cout<<"\n\n22x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==7))
        {
            std::cout<<"\n\n22x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==22)&&(y==8))
        {
            std::cout<<"\n\n22x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==9))
        {
            std::cout<<"\n\n22x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==22)&&(y==10))
        {
            std::cout<<"\n\n22x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==22)&&(y==11))
        {
            std::cout<<"\n\n22x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==12))
        {
            std::cout<<"\n\n22x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==13))
        {
            std::cout<<"\n\n22x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==14))
        {
            std::cout<<"\n\n22x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==15))
        {
            std::cout<<"\n\n22x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==16))
        {
            std::cout<<"\n\n22x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==17))
        {
            std::cout<<"\n\n22x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==21))
        {
            std::cout<<"\n\n22x21\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==22))
        {
            std::cout<<"\n\n22x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==23))
        {
            std::cout<<"\n\n22x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==24))
        {
            std::cout<<"\n\n22x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==25))
        {
            std::cout<<"\n\n22x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==26))
        {
            std::cout<<"\n\n22x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==27))
        {
            std::cout<<"\n\n22x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==28))
        {
            std::cout<<"\n\n22x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==29))
        {
            std::cout<<"\n\n22x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==30))
        {
            std::cout<<"\n\n22x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==31))
        {
            std::cout<<"\n\n22x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==32))
        {
            std::cout<<"\n\n22x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==33))
        {
            std::cout<<"\n\n22x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==22)&&(y==34))
        {
            std::cout<<"\n\n22x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=23***********************************************************************************
    {
        //****
        if ((x==23)&&(y==3))
        {
            std::cout<<"\n\n23x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==4))
        {
            std::cout<<"\n\n23x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==5))
        {
            std::cout<<"\n\n23x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==6))
        {
            std::cout<<"\n\n23x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==7))
        {
            std::cout<<"\n\n23x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==23)&&(y==8))
        {
            std::cout<<"\n\n23x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==9))
        {
            std::cout<<"\n\n23x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==23)&&(y==10))
        {
            std::cout<<"\n\n23x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==23)&&(y==11))
        {
            std::cout<<"\n\n23x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==12))
        {
            std::cout<<"\n\n23x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==13))
        {
            std::cout<<"\n\n23x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==14))
        {
            std::cout<<"\n\n23x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==15))
        {
            std::cout<<"\n\n23x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==16))
        {
            std::cout<<"\n\n23x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==17))
        {
            std::cout<<"\n\n23x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==18))
        {
            std::cout<<"\n\n23x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==22))
        {
            std::cout<<"\n\n23x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==23))
        {
            std::cout<<"\n\n23x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==24))
        {
            std::cout<<"\n\n23x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==25))
        {
            std::cout<<"\n\n23x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==26))
        {
            std::cout<<"\n\n23x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==27))
        {
            std::cout<<"\n\n23x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==28))
        {
            std::cout<<"\n\n23x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==29))
        {
            std::cout<<"\n\n23x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==30))
        {
            std::cout<<"\n\n23x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==31))
        {
            std::cout<<"\n\n23x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==32))
        {
            std::cout<<"\n\n23x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==33))
        {
            std::cout<<"\n\n23x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==23)&&(y==34))
        {
            std::cout<<"\n\n23x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=24***********************************************************************************
    {
        //****
        if ((x==24)&&(y==3))
        {
            std::cout<<"\n\n24x3\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==4))
        {
            std::cout<<"\n\n24x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==5))
        {
            std::cout<<"\n\n24x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==6))
        {
            std::cout<<"\n\n24x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==7))
        {
            std::cout<<"\n\n24x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==24)&&(y==8))
        {
            std::cout<<"\n\n24x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==9))
        {
            std::cout<<"\n\n24x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==24)&&(y==10))
        {
            std::cout<<"\n\n24x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==24)&&(y==11))
        {
            std::cout<<"\n\n24x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==12))
        {
            std::cout<<"\n\n24x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==13))
        {
            std::cout<<"\n\n24x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==14))
        {
            std::cout<<"\n\n24x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==15))
        {
            std::cout<<"\n\n24x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==16))
        {
            std::cout<<"\n\n24x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==17))
        {
            std::cout<<"\n\n24x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==18))
        {
            std::cout<<"\n\n24x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==19))
        {
            std::cout<<"\n\n24x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==22))
        {
            std::cout<<"\n\n24x22\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==23))
        {
            std::cout<<"\n\n24x23\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==24))
        {
            std::cout<<"\n\n24x24\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==25))
        {
            std::cout<<"\n\n24x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==26))
        {
            std::cout<<"\n\n24x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==27))
        {
            std::cout<<"\n\n24x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==28))
        {
            std::cout<<"\n\n24x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==29))
        {
            std::cout<<"\n\n24x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==30))
        {
            std::cout<<"\n\n24x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==31))
        {
            std::cout<<"\n\n24x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==32))
        {
            std::cout<<"\n\n24x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==33))
        {
            std::cout<<"\n\n24x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==34))
        {
            std::cout<<"\n\n24x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==35))
        {
            std::cout<<"\n\n24x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==36))
        {
            std::cout<<"\n\n24x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==37))
        {
            std::cout<<"\n\n24x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==38))
        {
            std::cout<<"\n\n24x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==24)&&(y==39))
        {
            std::cout<<"\n\n24x39\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=25***********************************************************************************
    {
        //****
        if ((x==25)&&(y==4))
        {
            std::cout<<"\n\n25x4\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==5))
        {
            std::cout<<"\n\n25x5\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==6))
        {
            std::cout<<"\n\n25x6\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==7))
        {
            std::cout<<"\n\n25x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==8))
        {
            std::cout<<"\n\n25x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==9))
        {
            std::cout<<"\n\n25x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==25)&&(y==10))
        {
            std::cout<<"\n\n25x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==25)&&(y==11))
        {
            std::cout<<"\n\n25x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==12))
        {
            std::cout<<"\n\n25x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==13))
        {
            std::cout<<"\n\n25x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==14))
        {
            std::cout<<"\n\n25x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==15))
        {
            std::cout<<"\n\n25x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==16))
        {
            std::cout<<"\n\n25x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==17))
        {
            std::cout<<"\n\n25x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==18))
        {
            std::cout<<"\n\n25x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==19))
        {
            std::cout<<"\n\n25x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==20))
        {
            std::cout<<"\n\n25x20\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==25))
        {
            std::cout<<"\n\n25x25\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==26))
        {
            std::cout<<"\n\n25x26\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==27))
        {
            std::cout<<"\n\n25x27\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==28))
        {
            std::cout<<"\n\n25x28\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==29))
        {
            std::cout<<"\n\n25x29\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==30))
        {
            std::cout<<"\n\n25x30\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==31))
        {
            std::cout<<"\n\n25x31\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==32))
        {
            std::cout<<"\n\n25x32\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==33))
        {
            std::cout<<"\n\n25x33\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==34))
        {
            std::cout<<"\n\n25x34\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==35))
        {
            std::cout<<"\n\n25x35\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==36))
        {
            std::cout<<"\n\n25x36\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==37))
        {
            std::cout<<"\n\n25x37\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==38))
        {
            std::cout<<"\n\n25x38\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==39))
        {
            std::cout<<"\n\n25x39\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==25)&&(y==40))
        {
            std::cout<<"\n\n25x40\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    //x=26***********************************************************************************
    {
        //****
        if ((x==26)&&(y==7))
        {
            std::cout<<"\n\n26x7\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==8))
        {
            std::cout<<"\n\n26x8\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==9))
        {
            std::cout<<"\n\n26x9\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==26)&&(y==10))
        {
            std::cout<<"\n\n26x10\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        if ((x==26)&&(y==11))
        {
            std::cout<<"\n\n26x11\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==12))
        {
            std::cout<<"\n\n26x12\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==13))
        {
            std::cout<<"\n\n26x13\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==14))
        {
            std::cout<<"\n\n26x14\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==15))
        {
            std::cout<<"\n\n26x15\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==16))
        {
            std::cout<<"\n\n26x16\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==17))
        {
            std::cout<<"\n\n26x17\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==18))
        {
            std::cout<<"\n\n26x18\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==19))
        {
            std::cout<<"\n\n26x19\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==20))
        {
            std::cout<<"\n\n26x20\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
        //****
        if ((x==26)&&(y==40))
        {
            std::cout<<"\n\n26x40\n";
            std::cout<<"What do you want to do?\n";
            input();
            checkLocation();
        }
    }
    
    input();
    
    return 0;
}



//Main*********************************************************************************
int main()
{
    std::cout<<"\n\n\n****PIXELESS****\nAlpha v5.1.2 On "<<OperatingSystem<<"\n\n";
    
    superUser=false;
    populateItemPList();
    
    askLoadGame();
    
    if (newGame==true)
    {
        intro();
    }
    
	checkLocation();
    
    return 0;
}

#include "GameWorld.h"
#include "Vehicle.h"
#include "constants.h"
#include "Obstacle.h"
#include "2d/Geometry.h"
#include "2d/Wall2D.h"
#include "2d/Transformations.h"
#include "SteeringBehaviors.h"
#include "time/PrecisionTimer.h"
#include "misc/Smoother.h"
#include "ParamLoader.h"
#include "misc/WindowUtils.h"
#include "misc/Stream_Utility_Functions.h"
#include <iostream>

// OUR Added classes
#include "AgentLeader.h"
#include "AgentPoursuiveur.h"

#include "resource.h"

#include <list>
using std::list;


//------------------------------- ctor -----------------------------------
//------------------------------------------------------------------------
GameWorld::GameWorld(int cx, int cy) :

	m_cxClient(cx),
	m_cyClient(cy),
	m_bPaused(false),
	m_vCrosshair(Vector2D(cxClient() / 2.0, cxClient() / 2.0)),
	m_bShowWalls(true),
	m_bShowObstacles(false),
	m_bShowPath(false),
	m_bShowWanderCircle(false),
	m_bShowSteeringForce(false),
	m_bShowFeelers(false),
	m_bShowDetectionBox(false),
	m_bShowFPS(true),
	m_dAvFrameTime(0),
	m_pPath(NULL),
	m_bRenderNeighbors(false),
	m_bViewKeys(false),
	m_bShowCellSpaceInfo(false),
	m_bManualControl(false),
	m_icx(cx),
	m_icy(cy)
{
	//create walls per default
	CreateWalls();

	//setup the spatial subdivision class
	m_pCellSpace = new CellSpacePartition<Vehicle*>((double)cx, (double)cy, Prm.NumCellsX, Prm.NumCellsY, Prm.NumAgents);

	double border = 30;
	m_pPath = new Path(5, border, border, cx - border, cy - border, true);

	CreatesAllVehicles();
}

void GameWorld::CreatesAllVehicles() {

	// CREATE ALL THE LEADERS
	for (int a = 0; a < Prm.NumLeaders; ++a)
	{
		//determine a random starting position
		Vector2D SpawnPos = Vector2D(m_icx / 2.0 + RandomClamped()*m_icx / 2.0,
			m_icy / 2.0 + RandomClamped()*m_icy / 2.0);

		AgentLeader* pLeader = new AgentLeader(this,
			SpawnPos,                 //initial position
			RandFloat()*TwoPi,        //start rotation
			Vector2D(0, 0),            //velocity
			Prm.VehicleMass,          //mass
			Prm.MaxSteeringForce,     //max force
			Prm.MaxSpeed,             //max velocity
			Prm.MaxTurnRatePerSecond, //max turn rate
			Prm.VehicleScale);        //scale

		// set the color depending on the number of the leader
		pLeader->setColorByInt(a);

		m_Vehicles.push_back(pLeader);
		m_Leaders.push_back(pLeader);

		//add it to the cell subdivision
		m_pCellSpace->AddEntity(pLeader);
	}

	// CREATE ALL THE AGENT POURSUIVEUR
	for (int a = 0; a < Prm.NumAgents; ++a)
	{

		//determine a random starting position
		Vector2D SpawnPos = Vector2D(m_icx / 2.0 + RandomClamped()*m_icx / 2.0,
			m_icy / 2.0 + RandomClamped()*m_icy / 2.0);


		AgentPoursuiveur* pPoursuiveur = new AgentPoursuiveur(this,
			SpawnPos,                 //initial position
			RandFloat()*TwoPi,        //start rotation
			Vector2D(0, 0),            //velocity
			Prm.VehicleMass,          //mass
			Prm.MaxSteeringForce,     //max force
			Prm.MaxSpeed,             //max velocity
			Prm.MaxTurnRatePerSecond, //max turn rate
			Prm.VehicleScale);        //scale

		m_Vehicles.push_back(pPoursuiveur);

		m_Poursuiveur.push_back(pPoursuiveur);

		//add it to the cell subdivision
		m_pCellSpace->AddEntity(pPoursuiveur);
	}
}


void GameWorld::ResetGame(HWND hwnd) {
	ChangeMenuState(hwnd, ID_BEHAVIOUR_FORMATIONV, MFS_UNCHECKED);
	ChangeMenuState(hwnd, ID_BEHAVIOUR_FILE, MFS_CHECKED);
	ChangeMenuState(hwnd, ID_MANUAL_MANUALON, MFS_UNCHECKED);

	for (unsigned int a = 0; a < m_Vehicles.size(); ++a)
	{
		delete m_Vehicles[a];
	}

	m_Vehicles.clear();
	m_Leaders.clear();
	m_Poursuiveur.clear();

	CreatesAllVehicles();
}

//-------------------------------- dtor ----------------------------------
//------------------------------------------------------------------------
GameWorld::~GameWorld()
{
	for (unsigned int a = 0; a < m_Vehicles.size(); ++a)
	{
		delete m_Vehicles[a];
	}

	for (unsigned int ob = 0; ob < m_Obstacles.size(); ++ob)
	{
		delete m_Obstacles[ob];
	}

	delete m_pCellSpace;

	delete m_pPath;
}


//----------------------------- Update -----------------------------------
//------------------------------------------------------------------------
void GameWorld::Update(double time_elapsed)
{
	if (m_bPaused) return;
	std::cout << "Output sentence 100";

	//create a smoother to smooth the framerate
	const int SampleRate = 10;
	static Smoother<double> FrameRateSmoother(SampleRate, 0.0);

	m_dAvFrameTime = FrameRateSmoother.Update(time_elapsed);


	//update the vehicles
	for (unsigned int a = 0; a < m_Vehicles.size(); ++a)
	{
		m_Vehicles[a]->Update(time_elapsed);
	}
}


//--------------------------- CreateWalls --------------------------------
//
//  creates some walls that form an enclosure for the steering agents.
//  used to demonstrate several of the steering behaviors
//------------------------------------------------------------------------
void GameWorld::CreateWalls()
{
	//create the walls  
	double bordersize = 20.0;
	double CornerSize = 0.2;
	double vDist = m_cyClient - 2 * bordersize;
	double hDist = m_cxClient - 2 * bordersize;

	const int NumWallVerts = 8;

	Vector2D walls[NumWallVerts] = { Vector2D(hDist*CornerSize + bordersize, bordersize),
									 Vector2D(m_cxClient - bordersize - hDist*CornerSize, bordersize),
									 Vector2D(m_cxClient - bordersize, bordersize + vDist*CornerSize),
									 Vector2D(m_cxClient - bordersize, m_cyClient - bordersize - vDist*CornerSize),

									 Vector2D(m_cxClient - bordersize - hDist*CornerSize, m_cyClient - bordersize),
									 Vector2D(hDist*CornerSize + bordersize, m_cyClient - bordersize),
									 Vector2D(bordersize, m_cyClient - bordersize - vDist*CornerSize),
									 Vector2D(bordersize, bordersize + vDist*CornerSize) };

	for (int w = 0; w < NumWallVerts - 1; ++w)
	{
		m_Walls.push_back(Wall2D(walls[w], walls[w + 1]));
	}

	m_Walls.push_back(Wall2D(walls[NumWallVerts - 1], walls[0]));
}


//--------------------------- CreateObstacles -----------------------------
//
//  Sets up the vector of obstacles with random positions and sizes. Makes
//  sure the obstacles do not overlap
//------------------------------------------------------------------------
void GameWorld::CreateObstacles()
{
	//create a number of randomly sized tiddlywinks
	for (int o = 0; o < Prm.NumObstacles; ++o)
	{
		bool bOverlapped = true;

		//keep creating tiddlywinks until we find one that doesn't overlap
		//any others.Sometimes this can get into an endless loop because the
		//obstacle has nowhere to fit. We test for this case and exit accordingly

		int NumTrys = 0; int NumAllowableTrys = 2000;

		while (bOverlapped)
		{
			NumTrys++;

			if (NumTrys > NumAllowableTrys) return;

			int radius = RandInt((int)Prm.MinObstacleRadius, (int)Prm.MaxObstacleRadius);

			const int border = 10;
			const int MinGapBetweenObstacles = 20;

			Obstacle* ob = new Obstacle(RandInt(radius + border, m_cxClient - radius - border),
				RandInt(radius + border, m_cyClient - radius - 30 - border),
				radius);

			if (!Overlapped(ob, m_Obstacles, MinGapBetweenObstacles))
			{
				//its not overlapped so we can add it
				m_Obstacles.push_back(ob);

				bOverlapped = false;
			}

			else
			{
				delete ob;
			}
		}
	}
}


//------------------------- Set Crosshair ------------------------------------
//
//  The user can set the position of the crosshair by right clicking the
//  mouse. This method makes sure the click is not inside any enabled
//  Obstacles and sets the position appropriately
//------------------------------------------------------------------------
void GameWorld::SetCrosshair(POINTS p)
{
	Vector2D ProposedPosition((double)p.x, (double)p.y);

	//make sure it's not inside an obstacle
	for (ObIt curOb = m_Obstacles.begin(); curOb != m_Obstacles.end(); ++curOb)
	{
		if (PointInCircle((*curOb)->Pos(), (*curOb)->BRadius(), ProposedPosition))
		{
			return;
		}

	}
	m_vCrosshair.x = (double)p.x;
	m_vCrosshair.y = (double)p.y;
}


//------------------------- HandleKeyPresses -----------------------------
void GameWorld::HandleKeyPresses(WPARAM wParam)
{

	switch (wParam)
	{
	case 'Q':
	{
		//TODO 
		//pLeader->Steering()->Manual(teta);
	}
	break;
	case 'D':
	{
		//TODO
		//pLeader->Steering()->Manual(teta);
	}
	break;
	case 'U':
	{
		delete m_pPath;
		double border = 60;
		m_pPath = new Path(RandInt(3, 7), border, border, cxClient() - border, cyClient() - border, true);
		m_bShowPath = true;
		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->Steering()->SetPath(m_pPath->GetPath());
		}
	}
	break;

	case 'P':

		TogglePause(); break;

	case 'O':

		ToggleRenderNeighbors(); break;

	case 'I':

	{
		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->ToggleSmoothing();
		}

	}

	break;

	case 'Y':

		m_bShowObstacles = !m_bShowObstacles;

		if (!m_bShowObstacles)
		{
			m_Obstacles.clear();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->ObstacleAvoidanceOff();
			}
		}
		else
		{
			CreateObstacles();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->ObstacleAvoidanceOn();
			}
		}
		break;

	}//end switch
}



//-------------------------- HandleMenuItems -----------------------------
void GameWorld::HandleMenuItems(WPARAM wParam, HWND hwnd)
{
	switch (wParam)
	{
	case ID_OB_OBSTACLES:

		m_bShowObstacles = !m_bShowObstacles;

		if (!m_bShowObstacles)
		{
			m_Obstacles.clear();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->ObstacleAvoidanceOff();
			}

			//uncheck the menu
			ChangeMenuState(hwnd, ID_OB_OBSTACLES, MFS_UNCHECKED);
		}
		else
		{
			CreateObstacles();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->ObstacleAvoidanceOn();
			}

			//check the menu
			ChangeMenuState(hwnd, ID_OB_OBSTACLES, MFS_CHECKED);
		}

		break;

	case ID_OB_WALLS:

		m_bShowWalls = !m_bShowWalls;

		if (m_bShowWalls)
		{
			CreateWalls();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->WallAvoidanceOn();
			}

			//check the menu
			ChangeMenuState(hwnd, ID_OB_WALLS, MFS_CHECKED);
		}

		else
		{
			m_Walls.clear();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_Vehicles[i]->Steering()->WallAvoidanceOff();
			}

			//uncheck the menu
			ChangeMenuState(hwnd, ID_OB_WALLS, MFS_UNCHECKED);
		}

		break;


	case IDR_PARTITIONING:
	{
		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->Steering()->ToggleSpacePartitioningOnOff();
		}

		//if toggled on, empty the cell space and then re-add all the 
		//vehicles
		if (m_Vehicles[0]->Steering()->isSpacePartitioningOn())
		{
			m_pCellSpace->EmptyCells();

			for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
			{
				m_pCellSpace->AddEntity(m_Vehicles[i]);
			}

			ChangeMenuState(hwnd, IDR_PARTITIONING, MFS_CHECKED);
		}
		else
		{
			ChangeMenuState(hwnd, IDR_PARTITIONING, MFS_UNCHECKED);
			ChangeMenuState(hwnd, IDM_PARTITION_VIEW_NEIGHBORS, MFS_UNCHECKED);
			m_bShowCellSpaceInfo = false;

		}
	}

	break;

	case IDM_PARTITION_VIEW_NEIGHBORS:
	{
		m_bShowCellSpaceInfo = !m_bShowCellSpaceInfo;

		if (m_bShowCellSpaceInfo)
		{
			ChangeMenuState(hwnd, IDM_PARTITION_VIEW_NEIGHBORS, MFS_CHECKED);

			if (!m_Vehicles[0]->Steering()->isSpacePartitioningOn())
			{
				SendMessage(hwnd, WM_COMMAND, IDR_PARTITIONING, NULL);
			}
		}
		else
		{
			ChangeMenuState(hwnd, IDM_PARTITION_VIEW_NEIGHBORS, MFS_UNCHECKED);
		}
	}
	break;


	case IDR_WEIGHTED_SUM:
	{
		ChangeMenuState(hwnd, IDR_WEIGHTED_SUM, MFS_CHECKED);
		ChangeMenuState(hwnd, IDR_PRIORITIZED, MFS_UNCHECKED);
		ChangeMenuState(hwnd, IDR_DITHERED, MFS_UNCHECKED);

		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->Steering()->SetSummingMethod(SteeringBehavior::weighted_average);
		}
	}

	break;

	case IDR_PRIORITIZED:
	{
		ChangeMenuState(hwnd, IDR_WEIGHTED_SUM, MFS_UNCHECKED);
		ChangeMenuState(hwnd, IDR_PRIORITIZED, MFS_CHECKED);
		ChangeMenuState(hwnd, IDR_DITHERED, MFS_UNCHECKED);

		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->Steering()->SetSummingMethod(SteeringBehavior::prioritized);
		}
	}

	break;

	case IDR_DITHERED:
	{
		ChangeMenuState(hwnd, IDR_WEIGHTED_SUM, MFS_UNCHECKED);
		ChangeMenuState(hwnd, IDR_PRIORITIZED, MFS_UNCHECKED);
		ChangeMenuState(hwnd, IDR_DITHERED, MFS_CHECKED);

		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->Steering()->SetSummingMethod(SteeringBehavior::dithered);
		}
	}

	break;

	case ID_MANUAL_MANUALON: {
		ToggleManualControl();

		CheckMenuItemAppropriately(hwnd, ID_MANUAL_MANUALON, m_bManualControl);

		if (m_bManualControl) {
			m_Leaders[0]->Steering()->WanderOff();
			m_Leaders[0]->Steering()->ManualOn();
			m_Leaders[0]->setColorByInt(3);
		}
		else {
			m_Leaders[0]->Steering()->WanderOn();
			m_Leaders[0]->Steering()->ManualOff();
			m_Leaders[0]->setColorByInt(0);
		}

		ToggleViewKeys();

		CheckMenuItemAppropriately(hwnd, ID_VIEW_KEYS, m_bViewKeys);
	}
	
	break;

	case ID_VIEW_KEYS:
	{
		ToggleViewKeys();

		CheckMenuItemAppropriately(hwnd, ID_VIEW_KEYS, m_bViewKeys);
	}

	break;

	case ID_VIEW_FPS:
	{
		ToggleShowFPS();

		CheckMenuItemAppropriately(hwnd, ID_VIEW_FPS, RenderFPS());
	}

	break;

	case ID_MENU_SMOOTHING:
	{
		for (unsigned int i = 0; i < m_Vehicles.size(); ++i)
		{
			m_Vehicles[i]->ToggleSmoothing();
		}

		CheckMenuItemAppropriately(hwnd, ID_MENU_SMOOTHING, m_Vehicles[0]->isSmoothingOn());
	}

	break;

	case ID_BEHAVIOUR_FORMATIONV:
	{
		ChangeMenuState(hwnd, ID_BEHAVIOUR_FORMATIONV, MFS_CHECKED);
		ChangeMenuState(hwnd, ID_BEHAVIOUR_FILE, MFS_UNCHECKED);

		for (unsigned int i = 0; i < m_Poursuiveur.size(); i++)
		{
			// we change the current behavior
			m_Poursuiveur[i]->SetBehavior(AgentPoursuiveur::Behavior::VFLOCKING);

			// then we udpate the position of all the agent, if he has a target
			if (m_Poursuiveur[i]->getTarget() != NULL) 
				m_Poursuiveur[i]->adaptBehavior();
		}
	}

	break;

	case ID_BEHAVIOUR_FILE:
	{
		ChangeMenuState(hwnd, ID_BEHAVIOUR_FORMATIONV, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_BEHAVIOUR_FILE, MFS_CHECKED);

		for (unsigned int i = 0; i < m_Poursuiveur.size(); i++)
		{
			// we change the current behavior
			m_Poursuiveur[i]->SetBehavior(AgentPoursuiveur::Behavior::INLINE);

			// then we udpate the position of all the agent
			if (m_Poursuiveur[i]->getTarget() != NULL)
				m_Poursuiveur[i]->adaptBehavior();
		}
	}

	break;

	case ID_LEADER_1LEADER: 
	{
		ChangeMenuState(hwnd, ID_LEADER_2LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_3LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_1LEADER, MFS_CHECKED);
		
		Prm.NumLeaders = 1;

		ResetGame(hwnd);
	}

	break;

	case ID_LEADER_2LEADER:
	{
		ChangeMenuState(hwnd, ID_LEADER_1LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_3LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_2LEADER, MFS_CHECKED);

		Prm.NumLeaders = 2;
		ResetGame(hwnd);
	}

	break;


	case ID_LEADER_3LEADER:
	{
		ChangeMenuState(hwnd, ID_LEADER_1LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_2LEADER, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_LEADER_3LEADER, MFS_CHECKED);

		Prm.NumLeaders = 3;
		ResetGame(hwnd);
	}

	break;

	case ID_POURSUIVEUR_10POURSUIVEURS:
	{
		ChangeMenuState(hwnd, ID_POURSUIVEUR_20POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_30POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_40POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_50POURSUIVEURS, MFS_UNCHECKED);

		ChangeMenuState(hwnd, ID_POURSUIVEUR_10POURSUIVEURS, MFS_CHECKED);

		Prm.NumAgents = 10;
		ResetGame(hwnd);
	}

	break;

	case ID_POURSUIVEUR_20POURSUIVEURS:
	{
		ChangeMenuState(hwnd, ID_POURSUIVEUR_10POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_30POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_40POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_50POURSUIVEURS, MFS_UNCHECKED);

		ChangeMenuState(hwnd, ID_POURSUIVEUR_20POURSUIVEURS, MFS_CHECKED);

		Prm.NumAgents = 20;
		ResetGame(hwnd);
	}

	break;

	case ID_POURSUIVEUR_30POURSUIVEURS:
	{
		ChangeMenuState(hwnd, ID_POURSUIVEUR_10POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_20POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_40POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_50POURSUIVEURS, MFS_UNCHECKED);

		ChangeMenuState(hwnd, ID_POURSUIVEUR_30POURSUIVEURS, MFS_CHECKED);

		Prm.NumAgents = 30;
		ResetGame(hwnd);
	}

	break;

	case ID_POURSUIVEUR_40POURSUIVEURS:
	{
		ChangeMenuState(hwnd, ID_POURSUIVEUR_10POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_20POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_30POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_50POURSUIVEURS, MFS_UNCHECKED);

		ChangeMenuState(hwnd, ID_POURSUIVEUR_40POURSUIVEURS, MFS_CHECKED);

		Prm.NumAgents = 40;
		ResetGame(hwnd);
	}

	break;

	case ID_POURSUIVEUR_50POURSUIVEURS:
	{
		ChangeMenuState(hwnd, ID_POURSUIVEUR_10POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_20POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_30POURSUIVEURS, MFS_UNCHECKED);
		ChangeMenuState(hwnd, ID_POURSUIVEUR_40POURSUIVEURS, MFS_UNCHECKED);

		ChangeMenuState(hwnd, ID_POURSUIVEUR_50POURSUIVEURS, MFS_CHECKED);

		Prm.NumAgents = 50;
		ResetGame(hwnd);
	}

	break;

	}//end switch
}


//------------------------------ Render ----------------------------------
//------------------------------------------------------------------------
void GameWorld::Render()
{
	gdi->TransparentText();

	//render any walls
	gdi->BlackPen();
	for (unsigned int w = 0; w < m_Walls.size(); ++w)
	{
		m_Walls[w].Render(true);  //true flag shows normals
	}

	//render any obstacles
	gdi->BlackPen();

	for (unsigned int ob = 0; ob < m_Obstacles.size(); ++ob)
	{
		gdi->Circle(m_Obstacles[ob]->Pos(), m_Obstacles[ob]->BRadius());
	}

	//render the agents
	for (unsigned int a = 0; a < m_Vehicles.size(); ++a)
	{
		m_Vehicles[a]->Render();

		//render cell partitioning stuff
		if (m_bShowCellSpaceInfo && a == 0)
		{
			gdi->HollowBrush();
			InvertedAABBox2D box(m_Vehicles[a]->Pos() - Vector2D(Prm.ViewDistance, Prm.ViewDistance),
				m_Vehicles[a]->Pos() + Vector2D(Prm.ViewDistance, Prm.ViewDistance));
			box.Render();

			gdi->RedPen();
			CellSpace()->CalculateNeighbors(m_Vehicles[a]->Pos(), Prm.ViewDistance);
			for (BaseGameEntity* pV = CellSpace()->begin(); !CellSpace()->end(); pV = CellSpace()->next())
			{
				gdi->Circle(pV->Pos(), pV->BRadius());
			}

			gdi->GreenPen();
			gdi->Circle(m_Vehicles[a]->Pos(), Prm.ViewDistance);
		}
	}

	//#define CROSSHAIR
#ifdef CROSSHAIR
  //and finally the crosshair
	gdi->RedPen();
	gdi->Circle(m_vCrosshair, 4);
	gdi->Line(m_vCrosshair.x - 8, m_vCrosshair.y, m_vCrosshair.x + 8, m_vCrosshair.y);
	gdi->Line(m_vCrosshair.x, m_vCrosshair.y - 8, m_vCrosshair.x, m_vCrosshair.y + 8);
	gdi->TextAtPos(5, cyClient() - 20, "Click to move crosshair");
#endif


	//gdi->TextAtPos(cxClient() -120, cyClient() - 20, "Press R to reset");

	gdi->TextColor(Cgdi::grey);
	if (RenderPath())
	{
		gdi->TextAtPos((int)(cxClient() / 2.0f - 80), cyClient() - 20, "Press 'U' for random path");

		m_pPath->Render();
	}

	if (RenderFPS())
	{
		gdi->TextColor(Cgdi::grey);
		gdi->TextAtPos(5, cyClient() - 20, ttos(1.0 / m_dAvFrameTime));
	}

	if (m_bShowCellSpaceInfo)
	{
		m_pCellSpace->RenderCells();
	}

}

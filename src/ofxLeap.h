//ofxLeap - Written by Reza Ali - www.syedrezaali.com based on Code from Theo Watson - http://theowatson.com
//Experimental and Likely to break and change on every commit
//Do not depend on this. 

#pragma once

#include "ofMain.h"
#include "Leap.h"
#include "Poco/Mutex.h"

using namespace Leap;

class ofxLeapSimpleHand{
    public:
    
    typedef struct{
        ofPoint pos;
        ofPoint vel;
        int64_t id; 
    }simpleFinger;
    
    vector <simpleFinger>  fingers;
    
    ofPoint handPos; 
    ofPoint handNormal; 
    
    void debugDraw(){
        ofPushStyle();
        
            ofSetColor(190);
            ofSetLineWidth(2);

            ofEnableLighting();
            ofPushMatrix();
                ofTranslate(handPos);
                //rotate the hand by the downwards normal
                ofQuaternion q;
                q.makeRotate(ofPoint(0, -1, 0), handNormal);
                ofMatrix4x4 m;
                q.get(m);
                glMultMatrixf(m.getPtr());
                
                
                //scale it to make it not a box
                ofScale(1, 0.35, 1.0);
                ofBox(0, 0, 0, 60);
            ofPopMatrix();
        
            
            for(int i = 0; i < fingers.size(); i++){
                ofDrawArrow(handPos, fingers[i].pos, 10);
            }
            
            ofSetColor(220, 220, 0);
            for(int i = 0; i < fingers.size(); i++){
                ofDrawArrow(fingers[i].pos + fingers[i].vel/20, fingers[i].pos + fingers[i].vel/10, 10);
            }
            
        ofPopStyle();
    }
};

class ofxLeap : public Leap::Listener {
	public:
	
		ofxLeap(){
            reset();
            resetMapping();
			ourController = new Leap::Controller();            
		}
        
        void reset(){
            currentFrameID = 0;
            preFrameId = -1;
        }

		~ofxLeap(){
			//note we don't delete the controller as it causes a crash / mutex exception. 
			close();
		}
		
		void close(){
			if( ourController ){
				ourController->removeListener(*this);
			}
		}

		//--------------------------------------------------------------
		void open(){
            reset();
			ourController->addListener(*this);
		}
		
        bool isConnected()
        {
            return ourController->isConnected();
        }
		//--------------------------------------------------------------
		virtual void onInit(const Leap::Controller& controller){
			ofLogVerbose("ofxLeapApp - onInit");
            const Leap::ScreenList sl = controller.calibratedScreens();
            cout << "# of Screens: " << sl.count() << endl;
            cout << "Screen #1 is Valid: " << sl[0].isValid() << endl;
            cout << "Screen #1 Width: " << sl[0].widthPixels() << endl;
            cout << "Screen #1 Height: " << sl[0].heightPixels() << endl;
            ourScreen = sl[0];
		}

		//--------------------------------------------------------------
		virtual void onConnect(const Leap::Controller& controller){
			ofLogVerbose("ofxLeapApp - onConnect");
		}

		//--------------------------------------------------------------
		virtual void onDisconnect(const Leap::Controller& controller){
			ofLogVerbose("ofxLeapApp - onDisconnect"); 
		}
		
		//if you want to use the Leap Controller directly - inhereit ofxLeap and implement this function
		//note: this function is called in a seperate thread - so GL commands here will cause the app to crash. 
		//-------------------------------------------------------------- 
		virtual void onFrame(const Leap::Controller& controller){
			ofLogVerbose("ofxLeapApp - onFrame");
			onFrameInternal(controller); // call this if you want to use getHands() / isFrameNew() etc
		}
		
		//Simple access to the hands 
		//--------------------------------------------------------------
		vector <Leap::Hand> getLeapHands(){
		
			vector <Leap::Hand> handsCopy; 
			if( ourMutex.tryLock(2000) ){
				handsCopy = hands;
				ourMutex.unlock();
			}

			return handsCopy;
		}

		//-------------------------------------------------------------- 
		vector <ofxLeapSimpleHand> getSimpleHands(){
		
			vector <ofxLeapSimpleHand> simpleHands; 
			vector <Leap::Hand> leapHands = getLeapHands();
            
            for(int i = 0; i < leapHands.size(); i++){
                ofxLeapSimpleHand curHand;
            
                curHand.handPos     = getMappedofPoint( leapHands[i].palmPosition() );
                curHand.handNormal  = getofPoint( leapHands[i].palmNormal() );

                for(int j = 0; j < leapHands[i].fingers().count(); j++){
                    const Leap::Finger & finger = hands[i].fingers()[j];
                
                    ofxLeapSimpleHand::simpleFinger f; 
                    f.pos = getMappedofPoint( finger.tipPosition() );
                    f.vel = getMappedofPoint(finger.tipVelocity());
                    f.id = finger.id();
                    
                    curHand.fingers.push_back( f );                    
                }
                
                simpleHands.push_back( curHand ); 
            }

			return simpleHands;
		}

		//-------------------------------------------------------------- 
		bool isFrameNew(){
			return currentFrameID != preFrameId;
		}

		//-------------------------------------------------------------- 
		void markFrameAsOld(){
			preFrameId = currentFrameID; 
		}
		
		//-------------------------------------------------------------- 
		int64_t getCurrentFrameID(){
			return currentFrameID;
		}
        
		//-------------------------------------------------------------- 
        void resetMapping(){
            xOffsetIn = 0;
			yOffsetIn = 0;
			zOffsetIn = 0;

			xOffsetOut = 0;
			yOffsetOut = 0;
			zOffsetOut = 0;

			xScale = 1;
			yScale = 1;
			zScale = 1;
        }
		
		//-------------------------------------------------------------- 
		void setMappingX(float minX, float maxX, float outputMinX, float outputMaxX){
			xOffsetIn	= minX;
			xOffsetOut	= outputMinX;			 
			xScale  =   ( outputMaxX - outputMinX ) / ( maxX - minX ); 
		}
		
		//-------------------------------------------------------------- 
		void setMappingY(float minY, float maxY, float outputMinY, float outputMaxY){
			yOffsetIn	= minY;
			yOffsetOut	= outputMinY;			 
			yScale  =   ( outputMaxY - outputMinY ) / ( maxY - minY ); 
		}

		//-------------------------------------------------------------- 
		void setMappingZ(float minZ, float maxZ, float outputMinZ, float outputMaxZ){
			zOffsetIn	= minZ;
			zOffsetOut	= outputMinZ;
			zScale  =   ( outputMaxZ - outputMinZ ) / ( maxZ - minZ ); 
		}

        ofPoint getScreenIntesectPosition(const Leap::Pointable& pointable, bool normalize, float clampRatio = 1.0f)
        {
            ofPoint p = getofPoint(ourScreen.intersect(pointable, normalize, clampRatio));
            p.x = xOffsetOut + (p.x - xOffsetIn) * xScale;
            p.y = yOffsetOut + (p.y - yOffsetIn) * yScale;
            p.z = zOffsetOut + (p.z - zOffsetIn) * zScale;
            return p;                                   
        }
    
		//helper function for converting a Leap::Vector to an ofPoint with a mapping
		//-------------------------------------------------------------- 
		ofPoint getMappedofPoint( Leap::Vector v ){
			ofPoint p = getofPoint(v);
			p.x = xOffsetOut + (p.x - xOffsetIn) * xScale;
			p.y = yOffsetOut + (p.y - yOffsetIn) * yScale;
			p.z = zOffsetOut + (p.z - zOffsetIn) * zScale;

			return p;
		}
		
		//helper function for converting a Leap::Vector to an ofPoint
		//-------------------------------------------------------------- 
		ofPoint getofPoint(Leap::Vector v){
			return ofPoint(v.x, v.y, v.z); 
		}

		protected:
			
			//if you want to use the Leap Controller directly - inhereit ofxLeap and implement this function
			//note: this function is called in a seperate thread - so GL commands here will cause the app to crash. 
			//-------------------------------------------------------------- 
			virtual void onFrameInternal(const Leap::Controller& contr){
				ourMutex.lock();
					const Leap::Frame & curFrame	= contr.frame();
					const Leap::HandList & handList	= curFrame.hands();
                    hands.clear();
					for(int i = 0; i < handList.count(); i++){
						hands.push_back( handList[i] );
					}
					currentFrameID = curFrame.id();
				
				ourMutex.unlock();
			}
			
			int64_t currentFrameID;
			int64_t preFrameId;
			
			float xOffsetIn, xOffsetOut, xScale;
			float yOffsetIn, yOffsetOut, yScale;
			float zOffsetIn, zOffsetOut, zScale;
			 
			vector <Leap::Hand> hands; 
			Leap::Controller * ourController;
            Leap::Screen ourScreen;
			Poco::FastMutex ourMutex;
};



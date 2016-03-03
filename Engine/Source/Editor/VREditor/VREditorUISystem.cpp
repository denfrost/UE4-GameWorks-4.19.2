// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorUISystem.h"
#include "VREditorMode.h"
#include "VREditorFloatingUI.h"
#include "VREditorDockableWindow.h"
#include "VREditorQuickMenu.h"
#include "VREditorRadialMenu.h"
#include "VREditorRadialMenuItem.h"
#include "VirtualHand.h"
#include "IHeadMountedDisplay.h"

// UI
#include "WidgetComponent.h"

// Content Browser
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "Editor/ContentBrowser/Public/IContentBrowserSingleton.h"

// World Outliner
#include "Editor/SceneOutliner/Public/SceneOutliner.h"

// Actor Details, Modes
#include "LevelEditor.h"

#include "SLevelViewport.h"
#include "SScaleBox.h"
#include "SDPIScaler.h"
#include "SWidget.h"

namespace VREd
{
	static FAutoConsoleVariable ContentBrowserUIResolutionX( TEXT( "VREd.ContentBrowserUIResolutionX" ), 1920, TEXT( "Horizontal resolution to use for content browser UI render targets" ) );
	static FAutoConsoleVariable ContentBrowserUIResolutionY( TEXT( "VREd.ContentBrowserUIResolutionY" ), 1200, TEXT( "Vertical resolution to use for content browser UI render targets" ) );
	static FAutoConsoleVariable EditorUIResolutionX( TEXT( "VREd.EditorUIResolutionX" ), 1024, TEXT( "Horizontal resolution to use for VR editor UI render targets" ) );
	static FAutoConsoleVariable EditorUIResolutionY( TEXT( "VREd.EditorUIResolutionY" ), 1024, TEXT( "Vertical resolution to use for VR editor UI render targets" ) );
	static FAutoConsoleVariable QuickMenuUIResolutionX( TEXT( "VREd.QuickMenuUIResolutionX" ), 1024, TEXT( "Horizontal resolution to use for Quick Menu VR UI render targets" ) );
	static FAutoConsoleVariable QuickMenuUIResolutionY( TEXT( "VREd.QuickMenuUIResolutionY" ), 1130, TEXT( "Vertical resolution to use for Quick Menu VR UI render targets" ) );
	static FAutoConsoleVariable ContentBrowserUISize( TEXT( "VREd.ContentBrowserUISize" ), 60.0f, TEXT( "How big content browser UIs should be" ) );
	static FAutoConsoleVariable EditorUISize( TEXT( "VREd.EditorUISize" ), 50.0f, TEXT( "How big editor UIs should be" ) );
	static FAutoConsoleVariable ContentBrowserUIScale( TEXT( "VREd.ContentBrowserUIScale" ), 2.5f, TEXT( "How much to scale up (or down) the content browser for VR" ) );
	static FAutoConsoleVariable EditorUIScale( TEXT( "VREd.EditorUIScale" ), 2.5f, TEXT( "How much to scale up (or down) editor UIs for VR" ) );
	static FAutoConsoleVariable EditorUIOffsetFromHand( TEXT( "VREd.EditorUIOffsetFromHand" ), 12.0f, TEXT( "How far to offset editor UIs from the origin of the hand mesh" ) );
	static FAutoConsoleVariable RadialMenuFadeDelay( TEXT( "VREd.RadialMenuFadeDelay" ), 0.2f, TEXT( "The delay for the radial menu after selecting a button" ) );
	static FAutoConsoleVariable UIAbsoluteScrollSpeed( TEXT( "VREd.UIAbsoluteScrollSpeed" ), 8.0f, TEXT( "How fast the UI scrolls when dragging the touchpad" ) );
	static FAutoConsoleVariable UIRelativeScrollSpeed( TEXT( "VREd.UIRelativeScrollSpeed" ), 0.75f, TEXT( "How fast the UI scrolls when holding an analog stick" ) );
	static FAutoConsoleVariable MinUIScrollDeltaForInertia( TEXT( "VREd.MinUIScrollDeltaForInertia" ), 0.25f, TEXT( "Minimum amount of touch pad input before inertial UI scrolling kicks in" ) );
	static FAutoConsoleVariable MinDockDragDistance( TEXT( "VREd.MinDockDragDistance" ), 10.0f, TEXT( "Minimum amount of distance needed to drag dock from hand" ) );
	static FAutoConsoleVariable DoubleClickTime( TEXT( "VREd.DoubleClickTime" ), 0.25f, TEXT( "Minumum duration between clicks for a double click event to fire" ) );

	// Tutorial UI commands
	static FAutoConsoleVariable TutorialUIResolutionX( TEXT( "VREd.TutorialUI.Resolution.X" ), 1920, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUIResolutionY( TEXT( "VREd.TutorialUI.Resolution.Y" ), 1080, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUISize( TEXT( "VREd.TutorialUI.Size" ), 200, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUIYaw( TEXT( "VREd.TutorialUI.Yaw" ), 270, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUIPitch( TEXT( "VREd.TutorialUI.Pitch" ), 45, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUILocationX( TEXT( "VREd.TutorialUI.Location.X" ), 0, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUILocationY( TEXT( "VREd.TutorialUI.Location.Y" ), 200, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
	static FAutoConsoleVariable TutorialUILocationZ( TEXT( "VREd.TutorialUI.Location.Z" ), 40, TEXT( "Minumum duration between clicks for a double click event to fire" ) );
}


FVREditorUISystem::FVREditorUISystem( FVREditorMode& InitOwner )
	: Owner( InitOwner ),
	  FloatingUIs(),
	  QuickMenuUI( nullptr ),
	  QuickRadialMenu( nullptr ),
	  QuickMenuWidgetClass( nullptr ),
	  QuickRadialWidgetClass( nullptr ),
	  DraggingUI( nullptr ),
	  DraggingUIHandIndex( INDEX_NONE ),
	  DraggingUIOffsetTransform( FTransform::Identity ),
	  bPanelVisibilityToggle( false ),
	  RadialMenuHideDelayTime( 0.0f )
{
	// Register to find out about VR events
	Owner.OnVRAction().AddRaw( this, &FVREditorUISystem::OnVRAction );
	Owner.OnVRHoverUpdate().AddRaw( this, &FVREditorUISystem::OnVRHoverUpdate );

	EditorUIPanels.SetNumZeroed( (int32)EEditorUIPanel::TotalCount );

	QuickMenuWidgetClass = LoadClass<UVREditorQuickMenu>( nullptr, TEXT( "/Engine/VREditor/UI/VRQuickMenu.VRQuickMenu_C" ) );
	check( QuickMenuWidgetClass != nullptr );

	QuickRadialWidgetClass = LoadClass<UVREditorRadialMenu>(nullptr, TEXT("/Engine/VREditor/UI/VRRadialQuickMenu.VRRadialQuickMenu_C"));
	check(QuickRadialWidgetClass != nullptr);

	TutorialWidgetClass = LoadClass<UVREditorBaseUserWidget>( nullptr, TEXT( "/Engine/VREditor/Tutorial/UI_VR_Tutorial_00.UI_VR_Tutorial_00_C" ) );
	check( TutorialWidgetClass != nullptr );

	// Create all of our UI panels
	CreateUIs();
}



FVREditorUISystem::~FVREditorUISystem()
{
	Owner.OnVRAction().RemoveAll( this );
	Owner.OnVRHoverUpdate().RemoveAll( this );

	// Kill floating UIs
	{
		for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
		{
			if( FloatingUIPtr != nullptr )
			{
				FloatingUIPtr->Destroy( false, false );
				FloatingUIPtr = nullptr;
			}
		}

		FloatingUIs.Reset();
		EditorUIPanels.Reset();
		EditorUIPanels.SetNumZeroed( (int32)EEditorUIPanel::TotalCount );
		QuickMenuUI = nullptr;
		QuickRadialMenu = nullptr;
	}

	QuickMenuWidgetClass = nullptr;
	QuickRadialWidgetClass = nullptr;
}


void FVREditorUISystem::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
	{
		Collector.AddReferencedObject( FloatingUIPtr );
	}
	Collector.AddReferencedObject( QuickMenuWidgetClass );
	Collector.AddReferencedObject( QuickRadialWidgetClass );
	Collector.AddReferencedObject( TutorialWidgetClass );
}


void FVREditorUISystem::OnVRAction( FEditorViewportClient& ViewportClient, const FVRAction VRAction, const EInputEvent Event, bool& bIsInputCaptured, bool& bWasHandled )
{
	if( !bWasHandled )
	{

		if( VRAction.ActionType == EVRActionType::ConfirmRadialSelection )
		{
			FVirtualHand& Hand = Owner.GetVirtualHand( VRAction.HandIndex );
			if( Event == IE_Pressed )
			{
				UVREditorRadialMenu* RadialMenu = QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>();
				if( IsShowingRadialMenu( VRAction.HandIndex ) )
				{
					RadialMenu->Update( Owner.GetVirtualHand( VRAction.HandIndex ) );
				}
				
				RadialMenu->SelectCurrentItem( Hand );
			}

			bWasHandled = true;
		}
		else if( VRAction.ActionType == EVRActionType::SelectAndMove )
		{
			FVirtualHand& Hand = Owner.GetVirtualHand( VRAction.HandIndex );
			FVector LaserPointerStart, LaserPointerEnd;
			if( Owner.GetLaserPointer( VRAction.HandIndex, LaserPointerStart, LaserPointerEnd ) )
			{
				FHitResult HitResult = Owner.GetHitResultFromLaserPointer( VRAction.HandIndex );
				if( HitResult.Actor.IsValid()  )
				{
					// Only allow clicks to our own widget components
					UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>( HitResult.GetComponent() );
					if( WidgetComponent != nullptr && IsWidgetAnEditorUIWidget( WidgetComponent ) )
					{
						// Always mark the event as handled so that the editor doesn't try to select the widget component
						bWasHandled = true;

						if( Event != IE_Repeat )
						{
							UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>( HitResult.GetComponent() );
							if( WidgetComponent )
							{
								FVector2D LastLocalHitLocation = WidgetComponent->GetLastLocalHitLocation();

								FVector2D LocalHitLocation;
								WidgetComponent->GetLocalHitLocation( HitResult.ImpactPoint, LocalHitLocation );

								// If we weren't already hovering over this widget, then we'll reset the last hit location
								if( WidgetComponent != Hand.HoveringOverWidgetComponent )
								{
									LastLocalHitLocation = LocalHitLocation;
								}

								FWidgetPath WidgetPathUnderFinger = FWidgetPath( WidgetComponent->GetHitWidgetPath( HitResult.ImpactPoint, /*bIgnoreEnabledStatus*/ false ) );
								if( WidgetPathUnderFinger.IsValid() )
								{
									TSet<FKey> PressedButtons;
									if( Event == IE_Pressed )
									{
										PressedButtons.Add( EKeys::LeftMouseButton );
									}
									FPointerEvent PointerEvent(
										1 + VRAction.HandIndex,
										LocalHitLocation,
										LastLocalHitLocation,
										PressedButtons,
										EKeys::LeftMouseButton,
										0.0f,	// Wheel delta
										FModifierKeysState() );

									Hand.HoveringOverWidgetComponent = WidgetComponent;

									FReply Reply = FReply::Unhandled();
									if( Event == IE_Pressed )
									{
										Reply = FSlateApplication::Get().RoutePointerDownEvent( WidgetPathUnderFinger, PointerEvent );
										Hand.bIsClickingOnUI = true;
										bIsInputCaptured = true;
									}
									else if( Event == IE_Released )
									{
										Reply = FSlateApplication::Get().RoutePointerUpEvent( WidgetPathUnderFinger, PointerEvent );

										const double CurrentTime = FPlatformTime::Seconds();
										if( CurrentTime - Hand.LastClickReleaseTime <= VREd::DoubleClickTime->GetFloat() )
										{
											// Trigger a double click event!
											Reply = FSlateApplication::Get().RoutePointerDoubleClickEvent( WidgetPathUnderFinger, PointerEvent );
										}

										Hand.LastClickReleaseTime = CurrentTime;
									}
								}
							}
						}
					}
				}
			}

			if( Event == IE_Released )
			{
				if( Hand.bIsClickingOnUI )
				{
					Hand.bIsClickingOnUI = false;
					bIsInputCaptured = false;
				}

				if ( !bWasHandled )
				{
					TSet<FKey> PressedButtons;
					FPointerEvent PointerEvent(
						1 + VRAction.HandIndex,
						FVector2D::ZeroVector,
						FVector2D::ZeroVector,
						PressedButtons,
						EKeys::LeftMouseButton,
						0.0f,	// Wheel delta
						FModifierKeysState() );

					FWidgetPath EmptyWidgetPath;

					Hand.bIsClickingOnUI = false;
					FReply Reply = FSlateApplication::Get().RoutePointerUpEvent( EmptyWidgetPath, PointerEvent );
				}
			}
		}
	}

	// Stop dragging the dock if we are dragging a dock
	if (VRAction.ActionType == EVRActionType::SelectAndMove && Event == IE_Released)
	{
		// Put the Dock back on the hand it came from or leave it where it is in the room
		if (DraggingUI != nullptr && DraggingUIHandIndex == VRAction.HandIndex)
		{
			FVirtualHand& Hand = Owner.GetVirtualHand( VRAction.HandIndex );
			FVirtualHand& OtherHand = Owner.GetOtherHand( VRAction.HandIndex );
			const float Distance = FVector::Dist( Hand.HoverLocation, OtherHand.Transform.GetLocation() ) / Owner.GetWorldScaleFactor();
			if (Distance > VREd::MinDockDragDistance->GetFloat())
			{
				DraggingUI->SetDockedTo( AVREditorFloatingUI::EDockedTo::Room );
			}
			else
			{
				const int32 OtherHandIndex = Owner.GetOtherHandIndex( DraggingUIHandIndex );
				ShowEditorUIPanel( DraggingUI, OtherHandIndex, true, true );
			}

			StopDraggingDockUI();
		}
	}
}


void FVREditorUISystem::OnVRHoverUpdate( FEditorViewportClient& ViewportClient, const int32 HandIndex, FVector& HoverImpactPoint, bool& bIsHoveringOverUI, bool& bWasHandled )
{
	FVirtualHand& Hand = Owner.GetVirtualHand( HandIndex );

	if( !bWasHandled )
	{
		FVector LaserPointerStart, LaserPointerEnd;
		if( Owner.GetLaserPointer( HandIndex, LaserPointerStart, LaserPointerEnd ) )
		{
			FHitResult HitResult = Owner.GetHitResultFromLaserPointer( HandIndex );
			if( HitResult.Actor.IsValid() )
			{
				// Only allow clicks to our own widget components
				UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>( HitResult.GetComponent() );
				if( WidgetComponent != nullptr && IsWidgetAnEditorUIWidget( WidgetComponent ) )
				{
					FVector2D LastLocalHitLocation = WidgetComponent->GetLastLocalHitLocation();

					FVector2D LocalHitLocation;
					WidgetComponent->GetLocalHitLocation( HitResult.ImpactPoint, LocalHitLocation );

					// If we weren't already hovering over this widget, then we'll reset the last hit location
					if( WidgetComponent != Hand.HoveringOverWidgetComponent )
					{
						LastLocalHitLocation = LocalHitLocation;
					}

					// @todo vreditor UI: There is a CursorRadius optional arg that we migth want to make use of wherever we call GetHitWidgetPath()!
					FWidgetPath WidgetPathUnderFinger = FWidgetPath( WidgetComponent->GetHitWidgetPath( HitResult.ImpactPoint, /*bIgnoreEnabledStatus*/ false ) );
					if ( WidgetPathUnderFinger.IsValid() )
					{
						FVirtualHand& Hand = Owner.GetVirtualHand(HandIndex);
						Hand.bIsHovering = true;
						Hand.HoverLocation = HitResult.ImpactPoint;

						TSet<FKey> PressedButtons;
						FPointerEvent PointerEvent(
							1 + HandIndex,
							LocalHitLocation,
							LastLocalHitLocation,
							LocalHitLocation - LastLocalHitLocation,
							PressedButtons,
							FModifierKeysState());

						FSlateApplication::Get().RoutePointerMoveEvent(WidgetPathUnderFinger, PointerEvent, false);

						bWasHandled = true;
						HoverImpactPoint = HitResult.ImpactPoint;
						Hand.HoveringOverWidgetComponent = WidgetComponent;
						bIsHoveringOverUI = true;


						// Route the mouse scrolling
						if ( Hand.bIsTrackpadPositionValid[1] )
						{
							const bool bIsAbsolute = ( Owner.GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift );

							float ScrollDelta = 0.0f;
							if( Hand.bIsTouchingTrackpad || Owner.GetHMDDeviceType() != EHMDDeviceType::DT_SteamVR )
							{
								if( bIsAbsolute )
								{
									const float ScrollSpeed = VREd::UIRelativeScrollSpeed->GetFloat();
									ScrollDelta = Hand.TrackpadPosition.Y * ScrollSpeed;
								}
								else if( Hand.bIsTouchingTrackpad )
								{
									const float ScrollSpeed = VREd::UIAbsoluteScrollSpeed->GetFloat();
									ScrollDelta = ( Hand.LastTrackpadPosition.Y - Hand.TrackpadPosition.Y ) * ScrollSpeed;
								}
							}

							if( !FMath::IsNearlyZero( ScrollDelta ) )
							{
								FPointerEvent MouseWheelEvent(
									1 + HandIndex,
									LocalHitLocation,
									LastLocalHitLocation,
									PressedButtons,
									EKeys::MouseWheelAxis,
									ScrollDelta,
									FModifierKeysState() );

								FSlateApplication::Get().RouteMouseWheelOrGestureEvent(WidgetPathUnderFinger, MouseWheelEvent, nullptr);

								Hand.UIScrollVelocity = 0.0f;

								// Don't apply inertia unless the user dragged a decent amount this frame
								if( bIsAbsolute && FMath::Abs( ScrollDelta ) >= VREd::MinUIScrollDeltaForInertia->GetFloat() )
								{
									// Don't apply inertia if our data is sort of old
									const FTimespan CurrentTime = FTimespan::FromSeconds( FPlatformTime::Seconds() );
									if( CurrentTime - Hand.LastTrackpadPositionUpdateTime < FTimespan::FromSeconds( 1.0f / 30.0f ) )
									{
										//GWarn->Logf( TEXT( "INPUT: UIScrollVelocity=%0.2f" ), ScrollDelta );
										Hand.UIScrollVelocity = ScrollDelta;
									}
								}
							}
						}
						else
						{
							if( !FMath::IsNearlyZero( Hand.UIScrollVelocity ) )
							{
								// Apply UI scrolling inertia
								const float ScrollDelta = Hand.UIScrollVelocity;
								{
									FPointerEvent MouseWheelEvent(
										1 + HandIndex,
										LocalHitLocation,
										LastLocalHitLocation,
										PressedButtons,
										EKeys::MouseWheelAxis,
										ScrollDelta,
										FModifierKeysState() );

									FSlateApplication::Get().RouteMouseWheelOrGestureEvent( WidgetPathUnderFinger, MouseWheelEvent, nullptr );
								}

								// Apply damping
								FVector ScrollVelocityVector( Hand.UIScrollVelocity, 0.0f, 0.0f );
								const bool bVelocitySensitive = false;
								Owner.ApplyVelocityDamping( ScrollVelocityVector, bVelocitySensitive );
								Hand.UIScrollVelocity = ScrollVelocityVector.X;

								//GWarn->Logf( TEXT( "INERTIA: UIScrollVelocity==%0.2f  (DAMPING: UIScrollVelocity==%0.2f)" ), ScrollDelta, Hand.UIScrollVelocity );
							}
							else
							{
								Hand.UIScrollVelocity = 0.0f;
							}
						}
					}
				}
			}
		}
	}

	// If nothing was hovered, make sure we tell Slate about that
	if( !bWasHandled && Hand.HoveringOverWidgetComponent != nullptr )
	{
		const FVector2D LastLocalHitLocation = Hand.HoveringOverWidgetComponent->GetLastLocalHitLocation();
		Hand.HoveringOverWidgetComponent = nullptr;

		TSet<FKey> PressedButtons;
		FPointerEvent PointerEvent(
			1 + HandIndex,
			LastLocalHitLocation,
			LastLocalHitLocation,
			FVector2D::ZeroVector,
			PressedButtons,
			FModifierKeysState() );
		FSlateApplication::Get().RoutePointerMoveEvent( FWidgetPath(), PointerEvent, false );
	}
}


void FVREditorUISystem::Tick( FEditorViewportClient* ViewportClient, const float DeltaTime )
{
	// Figure out if one hand is "aiming toward" the other hand.  We'll fade in a UI on the hand being
	// aimed at when the user does this.
	if( QuickMenuUI != nullptr )
	{
		int32 HandIndexWithQuickMenu = INDEX_NONE;
		if( QuickMenuUI->IsUIVisible() )
		{
			HandIndexWithQuickMenu = QuickMenuUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftArm ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex;
		}

		const float WorldScaleFactor = Owner.GetWorldScaleFactor();

		int32 HandIndexThatNeedsQuickMenu = INDEX_NONE;
		for( int32 HandIndex = 0; HandIndex < VREditorConstants::NumVirtualHands; ++HandIndex )
		{
			bool bShowQuickMenuOnArm = false;

			const FVirtualHand& Hand = Owner.GetVirtualHand( HandIndex );
			const FVirtualHand& OtherHand = Owner.GetOtherHand( HandIndex );
			const int32 OtherHandIndex = Owner.GetOtherHandIndex( HandIndex );		

			// @todo vreditor tweak: Weird to hard code this here.  Probably should be an accessor on the hand itself, and based on the actual device type
			const FTransform UICapsuleTransform = OtherHand.Transform;
			const FVector UICapsuleStart = FVector( -6.0f, 0.0f, 0.0f ) * WorldScaleFactor;
			const FVector UICapsuleEnd = FVector( -14.0f, 0.0f, 0.0f ) * WorldScaleFactor;
			const float UICapsuleLocalRadius = 6.0f * WorldScaleFactor;
			const float MinDistanceToUICapsule = 8.0f * WorldScaleFactor;	// @todo vreditor tweak
			const FVector UIForwardVector = FVector::UpVector;
			const float MinDotForAimingAtOtherHand = 0.25f;	// @todo vreditor tweak

			if( Owner.IsHandAimingTowardsCapsule( HandIndex, UICapsuleTransform, UICapsuleStart, UICapsuleEnd, UICapsuleLocalRadius, MinDistanceToUICapsule, UIForwardVector, MinDotForAimingAtOtherHand ) )
			{
				bShowQuickMenuOnArm = true;
			}

			if( bShowQuickMenuOnArm )
			{
				HandIndexThatNeedsQuickMenu = OtherHandIndex;
			}
		}

		if( QuickMenuUI->IsUIVisible() )
		{
			// If we don't need a quick menu, or if a different hand needs to spawn it, then kill the existing menu
			if( HandIndexThatNeedsQuickMenu == INDEX_NONE || HandIndexThatNeedsQuickMenu != HandIndexWithQuickMenu  )
			{
				// Despawn
				Owner.GetVirtualHand( HandIndexWithQuickMenu ).bHasUIOnForearm = false;
				QuickMenuUI->ShowUI( false );
			}
		}

		if( HandIndexThatNeedsQuickMenu != INDEX_NONE && !QuickMenuUI->IsUIVisible() )
		{
			const float Size = 20.0f;
			const AVREditorFloatingUI::EDockedTo DockTo = ( HandIndexThatNeedsQuickMenu == VREditorConstants::LeftHandIndex ) ? AVREditorFloatingUI::EDockedTo::LeftArm : AVREditorFloatingUI::EDockedTo::RightArm;
			QuickMenuUI->SetDockedTo( DockTo );
			QuickMenuUI->ShowUI( true );
			Owner.GetVirtualHand( HandIndexThatNeedsQuickMenu ).bHasUIOnForearm = true;
		}
	}

	// If the user is moving the analog stick, try to spawn the radial menu for that hand
	if( Owner.GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift )
	{
		for( int32 HandIndex = 0; HandIndex < VREditorConstants::NumVirtualHands; ++HandIndex )
		{
			const FVirtualHand& Hand = Owner.GetVirtualHand( HandIndex );

			if( Hand.bIsTrackpadPositionValid[ 0 ] && Hand.bIsTrackpadPositionValid[ 1 ] )
			{
				if( Hand.TrackpadPosition.Size() > 0.25f )	// @todo vreditor tweak
				{
					TryToSpawnRadialMenu( HandIndex );
					
				}
				else
				{
					// Close it
					HideRadialMenu( HandIndex );
				}
			}
		}
	}
	else
	{
		// Close the radial menu if it was not updated for a while
		RadialMenuHideDelayTime += DeltaTime;
		if( RadialMenuHideDelayTime >= VREd::RadialMenuFadeDelay->GetFloat() )
		{
			HideRadialMenu( QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex );
		}
	}

	// Tick all of our floating UIs
	for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
	{
		if( FloatingUIPtr != nullptr )
		{
			FloatingUIPtr->TickManually( DeltaTime );
		}
	}
}

void FVREditorUISystem::Render( const FSceneView* SceneView, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	// ...
}

void FVREditorUISystem::CreateUIs()
{
	const FIntPoint DefaultResolution( VREd::EditorUIResolutionX->GetInt(), VREd::EditorUIResolutionY->GetInt() );

	// @todo vreditor: Tweak
	if ( QuickMenuWidgetClass != nullptr )
	{
		const FIntPoint Resolution( VREd::QuickMenuUIResolutionX->GetInt(), VREd::QuickMenuUIResolutionY->GetInt() );

		const bool bWithSceneComponent = false;
		QuickMenuUI = GetOwner().SpawnTransientSceneActor< AVREditorFloatingUI >(TEXT("QuickMenu"), bWithSceneComponent);
		QuickMenuUI->SetUMGWidget( *this, QuickMenuWidgetClass, Resolution, 22.0f, AVREditorFloatingUI::EDockedTo::Nothing );
		QuickMenuUI->ShowUI( false );
		QuickMenuUI->SetRelativeOffset( FVector( -10.0f, 0.0f, 3.0f ) );
		FloatingUIs.Add( QuickMenuUI );
	}

	// Create the radial UI
	if ( QuickRadialWidgetClass != nullptr )
	{
		TSharedPtr<SWidget> WidgetToDraw;
		QuickRadialMenu = GetOwner().SpawnTransientSceneActor< AVREditorFloatingUI >(TEXT("QuickRadialmenu"), false);
		QuickRadialMenu->SetUMGWidget( *this, QuickRadialWidgetClass, DefaultResolution, 40.0f, AVREditorFloatingUI::EDockedTo::Nothing );
		QuickRadialMenu->ShowUI( false );
		QuickRadialMenu->SetActorEnableCollision( false );
		QuickRadialMenu->SetRotateToHead( false );
		QuickRadialMenu->SetLocalRotation( FRotator( 45.0f, 180.0f, 0.0f) );
		QuickRadialMenu->SetRelativeOffset( FVector( 10.0f, 0.0f, 10.f ) );
		QuickRadialMenu->SetCollisionOnShow( false );
		FloatingUIs.Add(QuickRadialMenu);
	}

	// Make some editor UIs!
	{
		{
			const FIntPoint Resolution( VREd::ContentBrowserUIResolutionX->GetInt(), VREd::ContentBrowserUIResolutionY->GetInt() );

			IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>( "ContentBrowser" ).Get();;

			// @todo vreditor UI: We've turned off a LOT of content browser features that are clunky to use in VR right (pop ups, text fields, etc.)
			FContentBrowserConfig Config;
			Config.bCanShowClasses = false;
			Config.bCanShowRealTimeThumbnails = false;
			Config.InitialAssetViewType = EAssetViewType::Tile;
			Config.bCanShowDevelopersFolder = false;
			Config.bCanShowFolders = false;
			Config.bUsePathPicker = false;
			Config.bCanShowFilters = false;
			Config.bCanShowAssetSearch = false;
			Config.bUseSourcesView = true;
			Config.bExpandSourcesView = true;
			Config.ThumbnailLabel = EThumbnailLabel::NoLabel;
			Config.ThumbnailScale = 0.4f;

			const bool bIsVREditorDemo = FParse::Param( FCommandLine::Get(), TEXT( "VREditorDemo" ) );	// @todo vreditor: Remove this when no longer needed
			if( bIsVREditorDemo )
			{
				Config.bUsePathPicker = false;
				Config.bCanShowFilters = false;
				Config.bShowAssetPathTree = false;
				Config.bAlwaysShowCollections = true;
				Config.SelectedCollectionName.Name = FName( "Meshes" );	// @todo vreditor: hard-coded collection name and type
				Config.SelectedCollectionName.Type = ECollectionShareType::CST_Shared;
			}
			else
			{
				Config.bCanShowFilters = true;
				Config.bUsePathPicker = true;
				Config.bShowAssetPathTree = true;
				Config.bAlwaysShowCollections = false;
			}

			Config.bShowBottomToolbar = false;
			Config.bCanShowLockButton = false;
			TSharedRef<SWidget> ContentBrowser = ContentBrowserSingleton.CreateContentBrowser( "VRContentBrowser", nullptr, &Config );

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::ContentBrowserUIScale->GetFloat() )
				[
					ContentBrowser
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ContentBrowserUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ContentBrowserUI" ), bWithSceneComponent );
			ContentBrowserUI->SetSlateWidget( *this, WidgetToDraw, Resolution, VREd::ContentBrowserUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ContentBrowserUI->ShowUI( false );
			FloatingUIs.Add( ContentBrowserUI );

			EditorUIPanels[ (int32)EEditorUIPanel::ContentBrowser ] = ContentBrowserUI;
		}

		{
			FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );

			SceneOutliner::FInitializationOptions InitOptions;
			InitOptions.Mode = ESceneOutlinerMode::ActorBrowsing;

			TSharedRef<ISceneOutliner> SceneOutliner = SceneOutlinerModule.CreateSceneOutliner( InitOptions, FOnActorPicked() /* Not used for outliner when in browsing mode */ );
			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					SceneOutliner
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* WorldOutlinerUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "WorldOutlinerUI" ), bWithSceneComponent );
			WorldOutlinerUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			WorldOutlinerUI->ShowUI( false );
			FloatingUIs.Add( WorldOutlinerUI );

			EditorUIPanels[ (int32)EEditorUIPanel::WorldOutliner ] = WorldOutlinerUI;
		}

		{
			const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetFirstLevelEditor().ToSharedRef();

			const FName TabIdentifier = NAME_None;	// No tab for us!
			TSharedRef<SWidget> ActorDetails = LevelEditor->CreateActorDetails( TabIdentifier );

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					ActorDetails
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ActorDetailsUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ActorDetailsUI" ), bWithSceneComponent );
			ActorDetailsUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ActorDetailsUI->ShowUI( false );
			FloatingUIs.Add( ActorDetailsUI );

			EditorUIPanels[ (int32)EEditorUIPanel::ActorDetails ] = ActorDetailsUI;
		}

		{
			const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetFirstLevelEditor().ToSharedRef();

			TSharedRef<SWidget> Modes = LevelEditor->CreateToolBox();

			TSharedRef<SWidget> WidgetToDraw =
				SNew( SDPIScaler )
				.DPIScale( VREd::EditorUIScale->GetFloat() )
				[
					Modes
				]
			;

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* ModesUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "ModesUI" ), bWithSceneComponent );
			ModesUI->SetSlateWidget( *this, WidgetToDraw, DefaultResolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing );
			ModesUI->ShowUI( false );
			FloatingUIs.Add( ModesUI );

			// @todo vreditor placement: This is required to force the modes UI to refresh -- otherwise it looks empty
			GLevelEditorModeTools().ActivateMode( FBuiltinEditorModes::EM_Placement );

			EditorUIPanels[ (int32)EEditorUIPanel::Modes ] = ModesUI;			
		}

		// Create the tutorial dockable window
		{
			AVREditorFloatingUI* TutorialUI = GetOwner().SpawnTransientSceneActor< AVREditorDockableWindow >( TEXT( "TutorialUI" ), false );
			TutorialUI->SetUMGWidget( *this, TutorialWidgetClass, FIntPoint( VREd::TutorialUIResolutionX->GetFloat(), VREd::TutorialUIResolutionY->GetFloat() ), VREd::TutorialUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Room );
			TutorialUI->ShowUI( true );
			TutorialUI->SetRelativeOffset( FVector( VREd::TutorialUILocationX->GetFloat(), VREd::TutorialUILocationY->GetFloat(), VREd::TutorialUILocationZ->GetFloat() ) );
			TutorialUI->SetLocalRotation( FRotator( VREd::TutorialUIPitch->GetFloat(), VREd::TutorialUIYaw->GetFloat(), 0 ) );
			FloatingUIs.Add( TutorialUI );

			EditorUIPanels[ (int32)EEditorUIPanel::Tutorial ] = TutorialUI;		
		}

		{
			const FIntPoint Resolution(VREd::EditorUIResolutionX->GetInt(), VREd::EditorUIResolutionY->GetInt());

			const bool bWithSceneComponent = false;
			AVREditorFloatingUI* TabManagerUI = GetOwner().SpawnTransientSceneActor< AVREditorFloatingUI >(TEXT("TabManager"), bWithSceneComponent);
			TabManagerUI->SetSlateWidget(*this, SNullWidget::NullWidget, Resolution, VREd::EditorUISize->GetFloat(), AVREditorFloatingUI::EDockedTo::Nothing);
			TabManagerUI->ShowUI(true);
			TabManagerUI->SetRelativeOffset(FVector(0, 0, 1000));
			FloatingUIs.Add(TabManagerUI);

			TSharedPtr<SWindow> TabManagerWindow = TabManagerUI->GetWidgetComponent()->GetSlateWindow();
			TSharedRef<SWindow> TabManagerWindowRef = TabManagerWindow.ToSharedRef();
			ProxyTabManager = MakeShareable(new FProxyTabmanager(TabManagerWindowRef));

			TSharedRef<FTabManager::FLayout> LoadedLayout = FTabManager::NewLayout(TEXT("VRTabManager"));
			LoadedLayout->AddArea(FTabManager::NewPrimaryArea());

			TSharedPtr<SWidget> DockLayout = FGlobalTabmanager::Get()->RestoreFrom(LoadedLayout, TabManagerWindowRef, false);
			TabManagerWindowRef->SetContent(DockLayout.ToSharedRef());

			// We're going to start stealing tabs from the global tab manager inserting them into the world instead.
			FGlobalTabmanager::Get()->SetProxyTabManager(ProxyTabManager);
		}
	}
}

void FVREditorUISystem::CleanUpActorsBeforeMapChangeOrSimulate()
{
	for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
	{
		if( FloatingUIPtr != nullptr )
		{
			FloatingUIPtr->Destroy( false, false );
			FloatingUIPtr = nullptr;
		}
	}

	FloatingUIs.Reset();
	EditorUIPanels.Reset();
	QuickRadialMenu = nullptr;
	QuickMenuUI = nullptr;

	ProxyTabManager.Reset();

	// Remove the proxy tab manager, we don't want to steal tabs any more.
	FGlobalTabmanager::Get()->SetProxyTabManager(TSharedPtr<FProxyTabmanager>());
}


bool FVREditorUISystem::IsWidgetAnEditorUIWidget( const UActorComponent* WidgetComponent ) const
{
	if( WidgetComponent != nullptr && WidgetComponent->IsA( UWidgetComponent::StaticClass() ) )
	{
		for( AVREditorFloatingUI* FloatingUIPtr : FloatingUIs )
		{
			if( FloatingUIPtr != nullptr )
			{
				if( WidgetComponent == FloatingUIPtr->GetWidgetComponent() )
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool FVREditorUISystem::IsShowingEditorUIPanel( const EEditorUIPanel EditorUIPanel ) const
{
	AVREditorFloatingUI* Panel = EditorUIPanels[ (int32)EditorUIPanel ];
	if( Panel != nullptr )
	{
		return Panel->IsUIVisible();
	}

	return false;
}


void FVREditorUISystem::ShowEditorUIPanel( const UWidgetComponent* WidgetComponent, const int32 HandIndex, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu  )
{
	AVREditorFloatingUI* Panel = nullptr;
	for( AVREditorFloatingUI* CurrentPanel : EditorUIPanels )
	{
		if( CurrentPanel->GetWidgetComponent() == WidgetComponent )
		{
			Panel = CurrentPanel;
			break;
		}
	}

	ShowEditorUIPanel( Panel, HandIndex, bShouldShow, OnHand, bRefreshQuickMenu );
}


void FVREditorUISystem::ShowEditorUIPanel( const EEditorUIPanel EditorUIPanel, const int32 HandIndex, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu  )
{
	AVREditorFloatingUI* Panel = EditorUIPanels[ (int32)EditorUIPanel ];
	ShowEditorUIPanel( Panel, HandIndex, bShouldShow, OnHand, bRefreshQuickMenu );
}

void FVREditorUISystem::ShowEditorUIPanel( AVREditorFloatingUI* Panel, const int32 HandIndex, const bool bShouldShow, const bool OnHand, const bool bRefreshQuickMenu )
{
	if( Panel != nullptr )
	{
		AVREditorFloatingUI::EDockedTo DockTo = Panel->GetDockedTo();
		if( OnHand || DockTo == AVREditorFloatingUI::EDockedTo::Nothing || DockTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockTo == AVREditorFloatingUI::EDockedTo::RightHand )
		{
			// Hide any panels that are already shown on this hand
			for( int32 PanelIndex = 0; PanelIndex < (int32)EEditorUIPanel::TotalCount; ++PanelIndex )
			{
				AVREditorFloatingUI* OtherPanel = EditorUIPanels[PanelIndex];
				if( OtherPanel != nullptr && OtherPanel->IsUIVisible() && OtherPanel->GetDockedTo() != AVREditorFloatingUI::EDockedTo::Room )
				{
					const uint32 OtherPanelDockedToHandIndex = OtherPanel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex;
					if( OtherPanelDockedToHandIndex == HandIndex )
					{
						OtherPanel->ShowUI( false );
						OtherPanel->SetDockedTo( AVREditorFloatingUI::EDockedTo::Nothing );
						Owner.GetVirtualHand( OtherPanelDockedToHandIndex ).bHasUIInFront = false;
					}
				}
			}

			Owner.GetVirtualHand( HandIndex ).bHasUIInFront = bShouldShow;

			const AVREditorFloatingUI::EDockedTo DockedTo = HandIndex == VREditorConstants::LeftHandIndex ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
			Panel->SetDockedTo( DockedTo );

			const FVector EditorUIRelativeOffset( Panel->GetSize().Y * 0.5f + VREd::EditorUIOffsetFromHand->GetFloat(), 0.0f, 0.0f );
			Panel->SetRelativeOffset( EditorUIRelativeOffset );
			Panel->SetLocalRotation( FRotator( 90.0f, 180.0f, 0.0f ) );
		}

		Panel->ShowUI( bShouldShow );

		if( bRefreshQuickMenu && QuickMenuUI )
		{
			QuickMenuUI->GetUserWidget<UVREditorQuickMenu>()->RefreshUI();
		}
	}
}


bool FVREditorUISystem::IsShowingRadialMenu( const int32 HandIndex ) const
{
	int32 DockedToHandIndex = INDEX_NONE;
	DockedToHandIndex = QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex;
	return DockedToHandIndex == HandIndex && !QuickRadialMenu->bHidden;
}


void FVREditorUISystem::UpdateRadialMenu( const int32 HandIndex )
{
	if(QuickRadialMenu->bHidden)
	{
		QuickRadialMenu->ShowUI( true, false );
	}

	if(!QuickRadialMenu->bHidden)
	{
		RadialMenuHideDelayTime = 0.0f;
		QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>()->Update( Owner.GetVirtualHand( HandIndex ) );
	}
}


void FVREditorUISystem::TryToSpawnRadialMenu( const int32 HandIndex )
{
	FVirtualHand& Hand = Owner.GetVirtualHand( HandIndex );

	int32 DockedToHandIndex = INDEX_NONE;
	if( !QuickRadialMenu->bHidden )
	{
		DockedToHandIndex = QuickRadialMenu->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex;
	}

	bool bNeedsSpawn =
		( QuickRadialMenu->bHidden || DockedToHandIndex != HandIndex ) &&
		Hand.DraggingMode != EVREditorDraggingMode::ActorsAtLaserImpact &&	// Don't show radial menu if the hand is busy dragging something around
		Hand.DraggingMode != EVREditorDraggingMode::ActorsFreely &&
		Hand.DraggingMode != EVREditorDraggingMode::ActorsWithGizmo &&
		Hand.DraggingMode != EVREditorDraggingMode::AssistingDrag &&
		!Hand.bIsHoveringOverUI;	// Don't show radial menu when aiming at a UI  (too much clutter)

	UVREditorRadialMenu* RadialMenu = QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>();
	// We need to update the trackpad position in the radialmenu before checking if it can be used
	RadialMenu->Update( Hand );
	if( RadialMenu && RadialMenu->IsInMenuRadius() )
	{
		bNeedsSpawn = false;
	}

	if( bNeedsSpawn )
	{
		DockedToHandIndex = HandIndex;

		const AVREditorFloatingUI::EDockedTo DockedTo = DockedToHandIndex == VREditorConstants::LeftHandIndex ? AVREditorFloatingUI::EDockedTo::LeftHand : AVREditorFloatingUI::EDockedTo::RightHand;
		QuickRadialMenu->SetDockedTo( DockedTo );
		QuickRadialMenu->ShowUI( true );
	}
}


void FVREditorUISystem::HideRadialMenu( const int32 HandIndex )
{
	if( IsShowingRadialMenu( HandIndex ) )
	{
		UVREditorRadialMenu* RadialMenu = QuickRadialMenu->GetUserWidget<UVREditorRadialMenu>();
		QuickRadialMenu->ShowUI( false, true, VREd::RadialMenuFadeDelay->GetFloat() );
	}
}


FTransform FVREditorUISystem::MakeDockableUITransformOnLaser( AVREditorDockableWindow* InitDraggingDockUI, const int32 HandIndex, const float DockSelectDistance ) const
{
	const FVirtualHand& Hand = Owner.GetVirtualHand( HandIndex );

	const FVector NewLocation = Hand.Transform.GetLocation() + ( Hand.Transform.GetRotation().Vector().GetSafeNormal() * DockSelectDistance );
	
	FRotator NewRotation = ( Hand.Transform.GetLocation() - NewLocation ).ToOrientationRotator();
	NewRotation.Roll = -Hand.Transform.GetRotation().Rotator().Roll;
	
	const FTransform LaserImpactToWorld( NewRotation, NewLocation );
	return LaserImpactToWorld;
}


FTransform FVREditorUISystem::MakeDockableUITransform( AVREditorDockableWindow* InitDraggingDockUI, const int32 HandIndex, const float DockSelectDistance ) const
{
	const FTransform UIOnLaserToWorld = MakeDockableUITransformOnLaser( DraggingUI, HandIndex, DockSelectDistance );
	const FTransform UIToUIOnLaser = DraggingUIOffsetTransform;
	
	const FTransform UIToWorld = UIToUIOnLaser * UIOnLaserToWorld;
	return UIToWorld;
}


AVREditorFloatingUI* FVREditorUISystem::StartDraggingDockUI( AVREditorDockableWindow* InitDraggingDockUI, const int32 HandIndex, const float DockSelectDistance )
{
	AVREditorFloatingUI::EDockedTo DockTo = InitDraggingDockUI->GetDockedTo();
	if( DockTo == AVREditorFloatingUI::EDockedTo::LeftHand || DockTo == AVREditorFloatingUI::EDockedTo::RightHand )
	{
		Owner.GetOtherHand( HandIndex ).bHasUIInFront = false;
	}

	DraggingUIHandIndex = HandIndex;

	FTransform UIToWorld = InitDraggingDockUI->GetActorTransform();
	UIToWorld.SetScale3D( FVector( 1.0f ) );
	const FTransform WorldToUI = UIToWorld.Inverse();

	const FTransform UIOnLaserToWorld = MakeDockableUITransformOnLaser( InitDraggingDockUI, HandIndex, DockSelectDistance );
	const FTransform UIOnLaserToUI = UIOnLaserToWorld * WorldToUI;
	const FTransform UIToUIOnLaser = UIOnLaserToUI.Inverse();
	DraggingUIOffsetTransform = UIToUIOnLaser;

	DraggingUI = InitDraggingDockUI;
	DraggingUI->SetDockedTo( AVREditorFloatingUI::EDockedTo::Nothing );
	return DraggingUI;
}

AVREditorDockableWindow* FVREditorUISystem::GetDraggingDockUI() const
{
	return DraggingUI;
}

void FVREditorUISystem::StopDraggingDockUI()
{
	DraggingUI = nullptr;
	FVirtualHand& Hand = Owner.GetVirtualHand( DraggingUIHandIndex );
	Hand.DraggingMode = EVREditorDraggingMode::Nothing;
	DraggingUIHandIndex = INDEX_NONE;
}

bool FVREditorUISystem::IsDraggingDockUI()
{
	return DraggingUI != nullptr &&  DraggingUI->GetDockedTo() == AVREditorFloatingUI::EDockedTo::Nothing;
}

void FVREditorUISystem::TogglePanelsVisibility()
{
	bPanelVisibilityToggle = !bPanelVisibilityToggle;
	
	bool bOneHandUsed = false;
	for ( AVREditorFloatingUI* Panel : EditorUIPanels )
	{
		if ( Panel != nullptr && Panel->IsUIVisible() != bPanelVisibilityToggle )
		{
			bool bShouldSetNewVisibility = true;
			const AVREditorFloatingUI::EDockedTo DockedTo = Panel->GetDockedTo();
			
			// Prevent the panel from spawning on the hand if there is already a UI on the hand
			if( DockedTo == AVREditorFloatingUI::EDockedTo::LeftHand ||  DockedTo == AVREditorFloatingUI::EDockedTo::RightHand )
			{
				if( bOneHandUsed && bPanelVisibilityToggle )
				{
					bShouldSetNewVisibility = false;
				}
				else
				{
					const uint32 HandIndex = Panel->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftHand ? VREditorConstants::LeftHandIndex : VREditorConstants::RightHandIndex;
					Owner.GetVirtualHand( HandIndex ).bHasUIInFront = bPanelVisibilityToggle;
					bOneHandUsed = true;
				}
			}
			else if ( DockedTo == AVREditorFloatingUI::EDockedTo::Nothing )
			{
				bShouldSetNewVisibility = false;
			}
			
			if( bShouldSetNewVisibility )
			{
				Panel->ShowUI( bPanelVisibilityToggle );
			}
		}
	}

	if (QuickMenuUI)
	{
		QuickMenuUI->GetUserWidget<UVREditorQuickMenu>()->RefreshUI();
	}
}
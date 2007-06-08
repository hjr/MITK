/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Module:    $RCSfile$
Language:  C++
Date:      $Date$
Version:   $Revision$

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "mitkVolumeDataVtkMapper3D.h"
#include "mitkLevelWindow.h"
#include "mitkDataTreeNode.h"
#include "mitkProperties.h"
#include "mitkColorProperty.h"
#include "mitkOpenGLRenderer.h"
#include "mitkLookupTableProperty.h"
#include "mitkTransferFunctionProperty.h"
#include "mitkVtkRenderWindow.h"
#include "mitkRenderingManager.h"

#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkVolumeRayCastMapper.h>

#if (VTK_MAJOR_VERSION >= 5)
#include <vtkFixedPointVolumeRayCastMapper.h>
#endif

#include <vtkVolumeTextureMapper2D.h> 
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeRayCastCompositeFunction.h>
#include <vtkFiniteDifferenceGradientEstimator.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCallbackCommand.h>
#include <vtkImageShiftScale.h>
#include <vtkImageChangeInformation.h>
#include <vtkImageWriter.h>
#include <vtkImageData.h>
#include <vtkLODProp3D.h>
#include <vtkImageResample.h>
#include <vtkPlane.h>
#include <vtkImplicitPlaneWidget.h>

#include <itkMultiThreader.h>


const mitk::Image* mitk::VolumeDataVtkMapper3D::GetInput()
{
  return static_cast<const mitk::Image*> ( GetData() );
}

mitk::VolumeDataVtkMapper3D::VolumeDataVtkMapper3D()
{
  m_Firstcall = true;
  m_PlaneSet = false;
  
  m_T2DMapper =  vtkVolumeTextureMapper2D::New();
  m_HiResMapper = vtkVolumeRayCastMapper::New();
  m_HiResMapper->SetSampleDistance(0.5); // 4 rays for every pixel
#if (VTK_MAJOR_VERSION >= 5)
  m_FPRCMapper = vtkFixedPointVolumeRayCastMapper::New();
  m_FPRCMapper->SetNumberOfThreads( itk::MultiThreader::GetGlobalDefaultNumberOfThreads() );
  m_FPRCMapper->IntermixIntersectingGeometryOn();
  m_FPRCMapper->SetSampleDistance(5); // 1 ray for every 25 (5x5) pixels
#else
  m_T2DMapperHi =  vtkVolumeTextureMapper2D::New();
#endif

  m_HiResMapper->IntermixIntersectingGeometryOn();
  m_HiResMapper->SetNumberOfThreads( itk::MultiThreader::GetGlobalDefaultNumberOfThreads() );
  vtkVolumeRayCastCompositeFunction* compositeFunction = vtkVolumeRayCastCompositeFunction::New();
  m_HiResMapper->SetVolumeRayCastFunction(compositeFunction);
  compositeFunction->Delete();

  vtkFiniteDifferenceGradientEstimator* gradientEstimator = 
  vtkFiniteDifferenceGradientEstimator::New();
  m_HiResMapper->SetGradientEstimator(gradientEstimator);
  gradientEstimator->Delete();

  m_VolumePropertyLow = vtkVolumeProperty::New();
  m_VolumePropertyMed = vtkVolumeProperty::New();
  m_VolumePropertyHigh = vtkVolumeProperty::New(); 

  m_VolumeLOD = vtkLODProp3D::New();

  m_HiResID = m_VolumeLOD->AddLOD(m_HiResMapper,m_VolumePropertyHigh,0.0);

  m_LowResID = m_VolumeLOD->AddLOD(m_T2DMapper,m_VolumePropertyLow,0.0); // TextureMapper2D

#if (VTK_MAJOR_VERSION >= 5)
  m_FPRCID = m_VolumeLOD->AddLOD(m_FPRCMapper,m_VolumePropertyMed,0.0);
#else
  m_MedResID = m_VolumeLOD->AddLOD(m_T2DMapperHi,m_VolumePropertyMed,0.0); // TextureMapper2D
#endif

  m_Resampler = vtkImageResample::New();
  m_Resampler->SetAxisMagnificationFactor(0,0.25);
  m_Resampler->SetAxisMagnificationFactor(1,0.25);
  m_Resampler->SetAxisMagnificationFactor(2,0.25);
  
  // For abort rendering mechanism
  m_VolumeLOD->AutomaticLODSelectionOff();

  m_Prop3D = m_VolumeLOD;
  m_Prop3D->Register(NULL); 
  
  m_ImageCast = vtkImageShiftScale::New(); 
  m_ImageCast->SetOutputScalarTypeToUnsignedShort();
  m_ImageCast->ClampOverflowOn();

  m_UnitSpacingImageFilter = vtkImageChangeInformation::New();
  m_UnitSpacingImageFilter->SetInput(m_ImageCast->GetOutput());
  m_UnitSpacingImageFilter->SetOutputSpacing(1.0,1.0,1.0);
    
  mitk::RenderingManager::GetInstance()->SetNumberOfLOD(3); //how many LODs should be used
  
}


mitk::VolumeDataVtkMapper3D::~VolumeDataVtkMapper3D()
{
  m_UnitSpacingImageFilter->Delete();
  m_ImageCast->Delete();
  m_T2DMapper->Delete();

  m_HiResMapper->Delete();
#if (VTK_MAJOR_VERSION >= 5)
  m_FPRCMapper->Delete();
#else  
  m_T2DMapperHi->Delete();
#endif
  m_Resampler->Delete();
  m_VolumePropertyLow->Delete();
  m_VolumePropertyMed->Delete();
  m_VolumePropertyHigh->Delete();
  m_VolumeLOD->Delete();
}

void mitk::VolumeDataVtkMapper3D::AbortCallback(vtkObject *caller, unsigned long , void *, void *) {

  mitk::VtkRenderWindow* renderWindow = dynamic_cast<mitk::VtkRenderWindow*>(caller);
  if ( renderWindow )
  {
    BaseRenderer* renderer = renderWindow->GetMitkRenderer();
    renderer->InvokeEvent( itk::ProgressEvent() );
  }

}


void mitk::VolumeDataVtkMapper3D::EndCallback(vtkObject *caller, unsigned long , void *, void *){
  
  mitk::VtkRenderWindow* renderWindow = dynamic_cast<mitk::VtkRenderWindow*>(caller);
  if ( renderWindow )
  {
    BaseRenderer* renderer = renderWindow->GetMitkRenderer();
    renderer->InvokeEvent( itk::EndEvent() );
  }

}

void mitk::VolumeDataVtkMapper3D::StartCallback(vtkObject *caller, unsigned long , void *, void *){
  mitk::VtkRenderWindow* renderWindow = dynamic_cast<mitk::VtkRenderWindow*>(caller);
  if ( renderWindow )
  { 
    BaseRenderer* renderer = renderWindow->GetMitkRenderer();
    renderer->InvokeEvent( itk::StartEvent() );    
  }

}


void mitk::VolumeDataVtkMapper3D::GenerateData(mitk::BaseRenderer* renderer)
{ 
  SetPreferences();

  if(mitk::RenderingManager::GetInstance()->GetCurrentLOD() == 0)
  {
    m_VolumeLOD->SetSelectedLODID(m_LowResID);
    //std::cout<<" Low ";
  }
  else if(mitk::RenderingManager::GetInstance()->GetCurrentLOD() == 1)
  {
#if (VTK_MAJOR_VERSION >= 5)
    m_VolumeLOD->SetSelectedLODID(m_FPRCID);
#else
    m_VolumeLOD->SetSelectedLODID(m_MedResID);
#endif
    //std::cout<<" Med ";
  }
  else if(mitk::RenderingManager::GetInstance()->GetCurrentLOD() == 2)
  {
    m_VolumeLOD->SetSelectedLODID(m_HiResID);
    //std::cout<<" Hi ";
  }
  else{
    //std::cout<<"Could not get current LOD ";
  }


  if(IsVisible(renderer)==false ||
      GetDataTreeNode() == NULL || 
      dynamic_cast<mitk::BoolProperty*>(GetDataTreeNode()->GetProperty("volumerendering",renderer).GetPointer())==NULL ||  
      dynamic_cast<mitk::BoolProperty*>(GetDataTreeNode()->GetProperty("volumerendering",renderer).GetPointer())->GetValue() == false 
    )
  {
    //  FIXME: don't understand this 
    if (m_Prop3D) {
      //      std::cout << "visibility off" <<std::endl;

      m_Prop3D->VisibilityOff(); 
    }
    return;
  }
  if (m_Prop3D) {
    m_Prop3D->VisibilityOn();
  }

  mitk::Image* input  = const_cast<mitk::Image *>(this->GetInput());
  if((input==NULL) || (input->IsInitialized()==false))
    return;

  // FIXME: const_cast; maybe GetVtkImageData can be made const by using volatile    
  const TimeSlicedGeometry* inputtimegeometry = input->GetTimeSlicedGeometry();
  assert(inputtimegeometry!=NULL);

  const Geometry3D* worldgeometry = renderer->GetCurrentWorldGeometry();

  assert(worldgeometry!=NULL);

  int timestep=0;
  ScalarType time = worldgeometry->GetTimeBounds()[0];
  if(time>-ScalarTypeNumericTraits::max())
    timestep = inputtimegeometry->MSToTimeStep(time);

  if(inputtimegeometry->IsValidTime(timestep)==false)
    return;

  vtkImageData* inputData = input->GetVtkImageData(timestep);
  if(inputData==NULL)
    return;

  m_ImageCast->SetInput( inputData );

//  m_ImageCast->Update();    

  //trying to avoid update-problem, when size of input changed. Does not really help much.
  if(m_ImageCast->GetOutput()!=NULL)
  {
    int inputWE[6]; 
    inputData->GetWholeExtent(inputWE);
    int * outputWE=m_UnitSpacingImageFilter->GetOutput()->GetExtent();
    if(m_ImageCast->GetOutput()!=NULL && memcmp(inputWE, outputWE,sizeof(int)*6) != 0)
    {
//      m_ImageCast->GetOutput()->SetUpdateExtentToWholeExtent();
//      m_UnitSpacingImageFilter->GetOutput()->SetUpdateExtentToWholeExtent();
      m_UnitSpacingImageFilter->GetOutput()->SetUpdateExtent(inputWE);
      m_UnitSpacingImageFilter->UpdateWholeExtent();
    }
  }

  m_Resampler->SetInput(m_UnitSpacingImageFilter->GetOutput());  
    
  m_T2DMapper->SetInput(m_Resampler->GetOutput());
  m_HiResMapper->SetInput(m_UnitSpacingImageFilter->GetOutput());
#if (VTK_MAJOR_VERSION >= 5)
  m_FPRCMapper->SetInput(m_UnitSpacingImageFilter->GetOutput());
#else
  m_T2DMapperHi->SetInput(m_Resampler->GetOutput());
#endif
  vtkPiecewiseFunction *opacityTransferFunction;
  vtkPiecewiseFunction *gradientTransferFunction;
  vtkColorTransferFunction* colorTransferFunction;

  opacityTransferFunction  = vtkPiecewiseFunction::New();
  gradientTransferFunction  = vtkPiecewiseFunction::New();
  colorTransferFunction    = vtkColorTransferFunction::New();

  m_ImageCast->SetShift(0);
    
  mitk::LookupTableProperty::Pointer lookupTableProp;
  lookupTableProp = dynamic_cast<mitk::LookupTableProperty*>(this->GetDataTreeNode()->GetProperty("LookupTable").GetPointer());
  mitk::TransferFunctionProperty::Pointer transferFunctionProp = dynamic_cast<mitk::TransferFunctionProperty*>(this->GetDataTreeNode()->GetProperty("TransferFunction").GetPointer());
  if ( transferFunctionProp.IsNotNull() )   {

    opacityTransferFunction = transferFunctionProp->GetValue()->GetScalarOpacityFunction();
    colorTransferFunction = transferFunctionProp->GetValue()->GetColorTransferFunction();
    gradientTransferFunction = transferFunctionProp->GetValue()->GetGradientOpacityFunction();
  } else if (lookupTableProp.IsNotNull() )
  {
    lookupTableProp->GetLookupTable()->CreateColorTransferFunction(colorTransferFunction);
    colorTransferFunction->ClampingOn();
    lookupTableProp->GetLookupTable()->CreateGradientTransferFunction(gradientTransferFunction);
    gradientTransferFunction->ClampingOn();
    lookupTableProp->GetLookupTable()->CreateOpacityTransferFunction(opacityTransferFunction);
    opacityTransferFunction->ClampingOn();
  }
  else
  {
    mitk::LevelWindow levelWindow;
    int lw_min,lw_max;

    bool binary=false;
    GetDataTreeNode()->GetBoolProperty("binary", binary, renderer);
    if(binary)
    {
      lw_min=0; lw_max=2;
    }
    else
    if(GetLevelWindow(levelWindow,renderer,"levelWindow") || GetLevelWindow(levelWindow,renderer))
    {
      lw_min = (int)levelWindow.GetMin();
      lw_max = (int)levelWindow.GetMax();
      //      std::cout << "levwin:" << levelWindow << std::endl;
      if(lw_min<0)
      {
        m_ImageCast->SetShift(-lw_min);
        lw_max+=-lw_min;
        lw_min=0;
      }
    } 
    else 
    {
      lw_min = 0;
      lw_max = 255;
    }

    opacityTransferFunction->AddPoint( lw_min, 0.0 );
    opacityTransferFunction->AddPoint( lw_max, 0.8 );
    opacityTransferFunction->ClampingOn();

    gradientTransferFunction->AddPoint( lw_min, 0.0 );
    gradientTransferFunction->AddPoint( lw_max, 0.8 );
    gradientTransferFunction->ClampingOn();

    float rgb[3]={1.0f,1.0f,1.0f};
    // check for color prop and use it for rendering if it exists
    if(GetColor(rgb, renderer))
    {
      colorTransferFunction->AddRGBPoint( lw_min, 0.0, 0.0, 0.0 );
      colorTransferFunction->AddRGBPoint( (lw_min+lw_max)/2, rgb[0], rgb[1], rgb[2] );
      colorTransferFunction->AddRGBPoint( lw_max, rgb[0], rgb[1], rgb[2] );
    }
    else
    {
      colorTransferFunction->AddRGBPoint( lw_min, 0.0, 0.0, 0.0 );
      colorTransferFunction->AddRGBPoint( (lw_min+lw_max)/2, 1, 1, 0.0 );
      colorTransferFunction->AddRGBPoint( lw_max, 0.8, 0.2, 0 );
    }

    colorTransferFunction->ClampingOn();
  }
  m_VolumePropertyLow->SetColor( colorTransferFunction );
  m_VolumePropertyLow->SetScalarOpacity( opacityTransferFunction ); 
  m_VolumePropertyLow->SetGradientOpacity( gradientTransferFunction ); 
  m_VolumePropertyLow->SetInterpolationTypeToNearest();

  m_T2DMapper->SetMaximumNumberOfPlanes(100);
#if (VTK_MAJOR_VERSION < 5)
  m_T2DMapperHi->SetMaximumNumberOfPlanes(150);
#endif

  m_VolumePropertyMed->SetColor( colorTransferFunction );
  m_VolumePropertyMed->SetScalarOpacity( opacityTransferFunction ); 
  m_VolumePropertyMed->SetGradientOpacity( gradientTransferFunction ); 
  m_VolumePropertyMed->SetInterpolationTypeToLinear();

  m_VolumePropertyHigh->SetColor( colorTransferFunction );
  m_VolumePropertyHigh->SetScalarOpacity( opacityTransferFunction ); 
  m_VolumePropertyHigh->SetGradientOpacity( gradientTransferFunction ); 
  m_VolumePropertyHigh->SetInterpolationTypeToLinear();

  mitk::OpenGLRenderer* openGlRenderer = dynamic_cast<mitk::OpenGLRenderer*>(renderer);
  assert(openGlRenderer);
  vtkRenderWindow* vtkRendWin = (openGlRenderer->GetVtkRenderWindow());

  vtkRenderWindowInteractor* interactor = vtkRendWin->GetInteractor();
  interactor->SetDesiredUpdateRate(0.00001);
  interactor->SetStillUpdateRate(0.00001);
  
  if (vtkRendWin && m_Firstcall) 
  {

    vtkCallbackCommand* cbc = vtkCallbackCommand::New();
    cbc->SetCallback(mitk::VolumeDataVtkMapper3D::AbortCallback); 
    vtkRendWin->AddObserver(vtkCommand::AbortCheckEvent,cbc); 

    vtkCallbackCommand *startCommand = vtkCallbackCommand::New();
    startCommand->SetCallback( VolumeDataVtkMapper3D::StartCallback );
    vtkRendWin->AddObserver( vtkCommand::StartEvent, startCommand );

    vtkCallbackCommand *endCommand = vtkCallbackCommand::New();
    endCommand->SetCallback( VolumeDataVtkMapper3D::EndCallback );
    vtkRendWin->AddObserver( vtkCommand::EndEvent, endCommand );
    m_Firstcall=false;
    
    mitk::RenderingManager::GetInstance()->SetCurrentLOD(0);
    
    mitk::RenderingManager::GetInstance()->SetShading(true,0);
    mitk::RenderingManager::GetInstance()->SetShading(true,1);
    mitk::RenderingManager::GetInstance()->SetShading(true,2);
    
    m_ClippingPlane = vtkPlane::New();  
    m_PlaneWidget = vtkImplicitPlaneWidget::New();
    mitk::RenderingManager::GetInstance()->SetClippingPlaneStatus(false);
  } 
  else 
  {
    //std::cout << "no vtk renderwindow" << std::endl;
  }
  SetClippingPlane(interactor);
  
/*  std::cout<<"RenderTime LowRes (T2D): "<<m_VolumeLOD->GetLODEstimatedRenderTime(m_LowResID)<<std::endl;
#if (VTK_MAJOR_VERSION >= 5)
  std::cout<<"RenderTime FPRC: "<<m_VolumeLOD->GetLODEstimatedRenderTime(m_FPRCID)<<std::endl;
#else
  std::cout<<"RenderTime MedRes (T2DHi): "<<m_VolumeLOD->GetLODEstimatedRenderTime(m_MedResID)<<std::endl;
#endif
  std::cout<<"RenderTime HiRes(T3D): "<<m_VolumeLOD->GetLODEstimatedRenderTime(m_HiResID)<<std::endl;  
*/
}

/* Shading enabled / disabled */
void mitk::VolumeDataVtkMapper3D::SetPreferences()
{
  //LOD 0
  if(mitk::RenderingManager::GetInstance()->GetShading(0))
  {
    m_VolumePropertyLow->ShadeOn();
  }
  else
  {
    m_VolumePropertyLow->ShadeOff();
  }
  
  //LOD 1
  if(mitk::RenderingManager::GetInstance()->GetShading(1))
  {
    m_VolumePropertyMed->ShadeOn();
  }
  else
  {
    m_VolumePropertyMed->ShadeOff();
  }

  //LOD 2
  if(mitk::RenderingManager::GetInstance()->GetShading(2))
  {
    m_VolumePropertyHigh->ShadeOn();
  }
  else
  {
    m_VolumePropertyHigh->ShadeOff();
  }
   
}

/* Adds A Clipping Plane to the Mapper */
void mitk::VolumeDataVtkMapper3D::SetClippingPlane(vtkRenderWindowInteractor* interactor)
{
  
  if(mitk::RenderingManager::GetInstance()->GetClippingPlaneStatus()) //if clipping plane is enabled
  {
      
    if(!m_PlaneSet)
    {

    m_PlaneWidget->SetInteractor(interactor);
    m_PlaneWidget->SetPlaceFactor(1.0);
    m_PlaneWidget->SetInput(m_UnitSpacingImageFilter->GetOutput());
    m_PlaneWidget->OutlineTranslationOff();
#if (VTK_MAJOR_VERSION >= 5)
    m_PlaneWidget->ScaleEnabledOff();
#endif
    m_PlaneWidget->DrawPlaneOff();
    mitk::Image* input  = const_cast<mitk::Image *>(this->GetInput());
    
    /*std::cout<<"X: "<<input->GetDimension(0)<<" Y: "<<input->GetDimension(1)<<" Z: "<<input->GetDimension(2)<<std::endl;
    
    std::cout<<"X spacing: "<<input->GetVtkImageData()->GetSpacing()[0]<<" Y spacing: "<<input->GetVtkImageData()->GetSpacing()[1]<<" Z spacing: "<<input->GetVtkImageData()->GetSpacing()[2]<<std::endl;
    
    std::cout<<"X extend 1: "<<input->GetVtkImageData()->GetExtent()[0]<<" X extend 2: "<<input->GetVtkImageData()->GetExtent()[1]<<" Y extend 1: "<<input->GetVtkImageData()->GetExtent()[2]<<" Y extend 2: "<<input->GetVtkImageData()->GetExtent()[3]<<" Z extend 1: "<<input->GetVtkImageData()->GetExtent()[4]<<" Z extend 2: "<<input->GetVtkImageData()->GetExtent()[5]<<std::endl;
    
    std::cout<<"X origin: "<<input->GetVtkImageData()->GetOrigin()[0]<<" Y origin: "<<input->GetVtkImageData()->GetOrigin()[1]<<" Z origin: "<<input->GetVtkImageData()->GetOrigin()[2]<<std::endl;*/
    
    //TODO: PlaceWidget() muss noch an den Datensatz angepasst werden
    m_PlaneWidget->PlaceWidget(input->GetVtkImageData()->GetOrigin()[0],(input->GetDimension(0))*(input->GetVtkImageData()->GetSpacing()[0]),input->GetVtkImageData()->GetOrigin()[1],(input->GetDimension(1))*(input->GetVtkImageData()->GetSpacing()[1]),input->GetVtkImageData()->GetOrigin()[2],(input->GetDimension(2))*(input->GetVtkImageData()->GetSpacing()[2]));
    
    m_T2DMapper->AddClippingPlane(m_ClippingPlane);
#if (VTK_MAJOR_VERSION >= 5)
    m_FPRCMapper->AddClippingPlane(m_ClippingPlane);
#else  
    m_T2DMapperHi->AddClippingPlane(m_ClippingPlane);
#endif
    m_HiResMapper->AddClippingPlane(m_ClippingPlane);
    //std::cout<<"plane added"<<std::endl;
    }
    m_PlaneWidget->GetPlane(m_ClippingPlane);
    
    m_PlaneSet = true;
    
  }
  else //if clippingplane is disabled
  { 
    if(m_PlaneSet) //if plane exists
    {
    DelClippingPlane();
    //std::cout<<"Plane removed"<<std::endl;
    }
  }

}    

/* Removes the clipping plane */
void mitk::VolumeDataVtkMapper3D::DelClippingPlane(){

  m_T2DMapper->RemoveAllClippingPlanes();
#if (VTK_MAJOR_VERSION >= 5)
  m_FPRCMapper->RemoveAllClippingPlanes();
#else  
  m_T2DMapperHi->RemoveAllClippingPlanes();
#endif
  m_HiResMapper->RemoveAllClippingPlanes();
  m_PlaneSet = false;

}


void mitk::VolumeDataVtkMapper3D::ApplyProperties(vtkActor* /*actor*/, mitk::BaseRenderer* /*renderer*/)
{

}

/*============================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center (DKFZ)
All rights reserved.

Use of this source code is governed by a 3-clause BSD license that can be
found in the LICENSE file.

============================================================================*/

#ifndef mitkFastMarchingTool3D_h_Included
#define mitkFastMarchingTool3D_h_Included

#include "mitkAutoSegmentationTool.h"
#include "mitkDataNode.h"
#include "mitkPointSet.h"
#include "mitkPointSetDataInteractor.h"
#include "mitkToolCommand.h"
#include <MitkSegmentationExports.h>

#include "mitkMessage.h"

#include "itkImage.h"

// itk filter
#include "itkBinaryThresholdImageFilter.h"
#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkFastMarchingImageFilter.h"
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"
#include "itkSigmoidImageFilter.h"

namespace us
{
  class ModuleResource;
}

namespace mitk
{
  /**
    \brief FastMarching semgentation tool.

    The segmentation is done by setting one or more seed points on the image
    and adapting the time range and threshold. The pipeline is:
      Smoothing->GradientMagnitude->SigmoidFunction->FastMarching->Threshold
    The resulting binary image is seen as a segmentation of an object.

    For detailed documentation see ITK Software Guide section 9.3.1 Fast Marching Segmentation.
  */
  class MITKSEGMENTATION_EXPORT FastMarchingTool3D : public AutoSegmentationTool
  {
    mitkNewMessageMacro(Ready);

  public:
    mitkClassMacro(FastMarchingTool3D, AutoSegmentationTool);
    itkFactorylessNewMacro(Self);
    itkCloneMacro(Self);

      /* typedefs for itk pipeline */
      typedef float InternalPixelType;
    typedef itk::Image<InternalPixelType, 3> InternalImageType;
    typedef mitk::Tool::DefaultSegmentationDataType OutputPixelType;
    typedef itk::Image<OutputPixelType, 3> OutputImageType;

    typedef itk::BinaryThresholdImageFilter<InternalImageType, OutputImageType> ThresholdingFilterType;
    typedef itk::CurvatureAnisotropicDiffusionImageFilter<InternalImageType, InternalImageType> SmoothingFilterType;
    typedef itk::GradientMagnitudeRecursiveGaussianImageFilter<InternalImageType, InternalImageType> GradientFilterType;
    typedef itk::SigmoidImageFilter<InternalImageType, InternalImageType> SigmoidFilterType;
    typedef itk::FastMarchingImageFilter<InternalImageType, InternalImageType> FastMarchingFilterType;
    typedef FastMarchingFilterType::NodeContainer NodeContainer;
    typedef FastMarchingFilterType::NodeType NodeType;

    bool CanHandle(BaseData *referenceData) const override;

    /* icon stuff */
    const char **GetXPM() const override;
    const char *GetName() const override;
    us::ModuleResource GetIconResource() const override;

    /// \brief Set parameter used in Threshold filter.
    void SetUpperThreshold(double);

    /// \brief Set parameter used in Threshold filter.
    void SetLowerThreshold(double);

    /// \brief Set parameter used in Fast Marching filter.
    void SetStoppingValue(double);

    /// \brief Set parameter used in Gradient Magnitude filter.
    void SetSigma(double);

    /// \brief Set parameter used in Fast Marching filter.
    void SetAlpha(double);

    /// \brief Set parameter used in Fast Marching filter.
    void SetBeta(double);

    /// \brief Adds the feedback image to the current working image.
    virtual void ConfirmSegmentation();

    /// \brief Set the working time step.
    virtual void SetCurrentTimeStep(int t);

    /// \brief Clear all seed points.
    void ClearSeeds();

    /// \brief Updates the itk pipeline and shows the result of FastMarching.
    void Update();

  protected:
    FastMarchingTool3D();
    ~FastMarchingTool3D() override;

    void Activated() override;
    void Deactivated() override;
    virtual void Initialize();

    /// \brief Add point action of StateMachine pattern
    virtual void OnAddPoint();

    /// \brief Delete action of StateMachine pattern
    virtual void OnDelete();

    /// \brief Reset all relevant inputs of the itk pipeline.
    void Reset();

    mitk::ToolCommand::Pointer m_ProgressCommand;

    Image::Pointer m_ReferenceImage;

    bool m_NeedUpdate;

    int m_CurrentTimeStep;

    float m_LowerThreshold; // used in Threshold filter
    float m_UpperThreshold; // used in Threshold filter
    float m_StoppingValue;  // used in Fast Marching filter
    float m_Sigma;          // used in GradientMagnitude filter
    float m_Alpha;          // used in Sigmoid filter
    float m_Beta;           // used in Sigmoid filter

    NodeContainer::Pointer m_SeedContainer; // seed points for FastMarching

    InternalImageType::Pointer m_ReferenceImageAsITK; // the reference image as itk::Image

    mitk::DataNode::Pointer m_ResultImageNode; // holds the result as a preview image

    mitk::DataNode::Pointer m_SeedsAsPointSetNode; // used to visualize the seed points
    mitk::PointSet::Pointer m_SeedsAsPointSet;
    mitk::PointSetDataInteractor::Pointer m_SeedPointInteractor;
    unsigned int m_PointSetAddObserverTag;
    unsigned int m_PointSetRemoveObserverTag;

    ThresholdingFilterType::Pointer m_ThresholdFilter;
    SmoothingFilterType::Pointer m_SmoothFilter;
    GradientFilterType::Pointer m_GradientMagnitudeFilter;
    SigmoidFilterType::Pointer m_SigmoidFilter;
    FastMarchingFilterType::Pointer m_FastMarchingFilter;
  };

} // namespace

#endif

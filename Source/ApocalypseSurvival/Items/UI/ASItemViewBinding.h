#pragma once

#include "MVVMSubsystem.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"

/** Publish the existing model to the Manual context authored in the Widget Blueprint. */
namespace ASItemViewBinding
{
	inline bool HasView(const UUserWidget* Widget)
	{
		const UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
		return View && View->GetViewClass();
	}
	inline void Set(UUserWidget* Widget, FName Context, UMVVMViewModelBase* Model)
	{
		if (Widget->IsDesignTime()) return;
		if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget); View && View->GetViewClass())
			View->SetViewModel(Context, TScriptInterface<INotifyFieldValueChanged>(Model));
	}
}
